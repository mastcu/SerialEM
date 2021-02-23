// EMstatusWindow.cpp:    Shows a summary of the contents of 3 or 4 buffers
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "EMstatusWindow.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include "ShiftManager.h"
#include "ShiftCalibrator.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// EMstatusWindow dialog


EMstatusWindow::EMstatusWindow(CWnd* pParent /*=NULL*/)
  : CToolDlg(EMstatusWindow::IDD, pParent)
{
  SEMBuildTime(__DATE__, __TIME__);
  //{{AFX_DATA_INIT(EMstatusWindow)
  m_sSizeText = _T("");
  m_sStageText = _T("");
  m_sTiltText = _T("");
  m_strDefocus = _T("");
  //}}AFX_DATA_INIT
}


void EMstatusWindow::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(EMstatusWindow)
  DDX_Control(pDX, IDC_SPINCURRENTBUF, m_sbcChangeBuf);
  DDX_Text(pDX, IDC_SIZETEXT, m_sSizeText);
  DDX_Text(pDX, IDC_STAGETEXT, m_sStageText);
  DDX_Text(pDX, IDC_TILTTEXT, m_sTiltText);
  DDX_Text(pDX, IDC_STATDEFOCUS, m_strDefocus);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(EMstatusWindow, CToolDlg)
  //{{AFX_MSG_MAP(EMstatusWindow)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINCURRENTBUF, OnDeltaposSpin)
  ON_WM_CANCELMODE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// EMstatusWindow message handlers

void EMstatusWindow::OnDeltaposSpin(NMHDR* pNMHDR, LRESULT* pResult) 
{
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;

  mWinApp->mMainView->TryToChangeBuffer(mWinApp->GetImBufIndex() + 
    pNMUpDown->iDelta, pNMUpDown->iDelta);
  mWinApp->RestoreViewFocus();
  *pResult = 1; // Better forbid the change to keep it from running out
}

void EMstatusWindow::Update()
{
  int whichBuf = 0;
  int indStat = 1;
  int width, height;
  bool spaceIt;
  BOOL STEM = false;
  CameraParameters *camP;
  if (!mInitialized)
    return;
  mBufText[0] = (CStatic *)GetDlgItem(IDC_DISPLAYEDBUF);
  mBufText[1] = (CStatic *)GetDlgItem(IDC_OTHERBUF1);
  mBufText[2] = (CStatic *)GetDlgItem(IDC_OTHERBUF2);
  //mBufText[3] = (CStatic *)GetDlgItem(IDC_OTHERBUF3);
  int indCur = mWinApp->GetImBufIndex();

  // Loop on the buffers, keep track of status lines filled
  while (whichBuf < MAX_BUFFERS) {
    EMimageBuffer *buf = mImBufs + whichBuf;

    // If this is the displayed image, fill top line and the detailed info
    if (whichBuf == indCur) {
      if (buf->mCamera >= 0) {
        camP = mWinApp->GetCamParams() + buf->mCamera;
        STEM = camP->STEMcamera;
      }
      SetStatusText(0, whichBuf);
      m_sSizeText = "Size:";
      m_sStageText = "Stage:";
      m_sTiltText = "Tilt:";
      m_strDefocus = "Def:";
      if (buf->mImage) {
		    width = buf->mImage->getWidth();
		    height = buf->mImage->getHeight();
		    spaceIt = width < 10000 && height < 10000;
        m_sSizeText.Format("Size: %d%sx%s%d  %s %s", width, spaceIt ? " " : "", 
          spaceIt ? " " : "", height, STEM ? "samp" : "bin", (LPCTSTR)buf->BinningText());
        EMimageExtra *extra = (EMimageExtra *)buf->mImage->GetUserData();
        if (extra) {
          if (extra->m_fTilt > EXTRA_VALUE_TEST)
          m_sTiltText.Format("Tilt: %.2f", extra->m_fTilt);
          m_strDefocus.Format("Def: %.2f", buf->mDefocus);

          if (buf->mConSetUsed == MONTAGE_CONSET && 
            (buf->mCaptured > 0 || buf->mCaptured == BUFFER_MONTAGE_PRESCAN) && 
            !mWinApp->mShiftCalibrator->CalibratingIS()) {
            MontParam *param = mWinApp->GetMontParam();
            int xpc = extra->m_iMontageX / 
              (param->xFrame - param->xOverlap) + 1;
            int ypc = extra->m_iMontageY /
              (param->yFrame - param->yOverlap) + 1;
            m_sStageText.Format("Piece X: %d  Y: %d  Z: %d",
              xpc, ypc, extra->m_iMontageZ);
          } else if (extra->mStageX > EXTRA_VALUE_TEST)
            m_sStageText.Format("Stage: %.2f, %.2f", extra->mStageX, extra->mStageY);


        }
      } 
    } else if (indStat < MAX_STATUS) {
      BOOL doFill = true;
      if (!buf->mImage) {
        // If there is no image, count how many valid images are left to fill
        // the slot; use this buffer if there are not enough
        int nLeft = 0;
        for (int i = whichBuf + 1; i < MAX_BUFFERS; i++)
          if (mImBufs[i].mImage && i != indCur)
            nLeft++;
        doFill = nLeft < 4 - indStat;
      }

      // Fill the lower status line and advance pointers
      if (doFill) {
        SetStatusText(indStat, whichBuf);
        indStat++;
      }
    }
    whichBuf++;
  }
  UpdateData(FALSE);
}

void EMstatusWindow::SetStatusText(int indStat, int whichBuf)
{
  EMimageBuffer *buf = mImBufs + whichBuf;
  int setUsed = buf->mConSetUsed;
  char letter = 'A' + whichBuf;
  CString string;;
  int saveFlag = buf->GetSaveCopyFlag();
  if (!buf->mImage)
    string.Format("%c: Empty", letter);
  else if (saveFlag >= 0 && buf->mCaptured) {
    // If unsaved, say so and include name of camera mode
    if (saveFlag && buf->mCaptured > 0)
      string.Format("%c: %s%s", letter, "UNSAVED, ", mModeNames[setUsed]);
    else if (buf->mCaptured == BUFFER_PROCESSED || 
      buf->mCaptured == BUFFER_AUTOCOR_OVERVIEW)
      string.Format("%c: %s%s", letter, "Processed, ", mModeNames[setUsed]);
    else if (buf->mCaptured == BUFFER_CROPPED)
      string.Format("%c: %s%s", letter, "Cropped, ", mModeNames[setUsed]);
    else if (buf->mCaptured == BUFFER_FFT || buf->mCaptured == BUFFER_LIVE_FFT)
      string.Format("%c: %s%s", letter, "FFT, ", mModeNames[setUsed]);
    else if (buf->mCaptured == BUFFER_MONTAGE_OVERVIEW)
      string.Format("%c: %s", letter, "Montage Overview");
    else if (buf->mCaptured == BUFFER_PRESCAN_OVERVIEW)
      string.Format("%c: %s", letter, "Prescan Overview");
    else if (buf->mCaptured == BUFFER_MONTAGE_PRESCAN)
      string.Format("%c: %s", letter, "Prescan frame");
    else if (buf->mCaptured == BUFFER_MONTAGE_CENTER)
      string.Format("%c: %s", letter, "Montage Center");
    else if (buf->mCaptured == BUFFER_MONTAGE_PIECE)
      string.Format("%c: %s", letter, "Montage Piece");
    else if (buf->mCaptured == BUFFER_PRESCAN_CENTER)
      string.Format("%c: %s", letter, "Prescan Center");
    else if (buf->mCaptured == BUFFER_CALIBRATION)
      string.Format("%c: %s", letter, "Calibration image");
    else if (buf->mCaptured == BUFFER_TRACKING)
      string.Format("%c: %s", letter, "Tracking image");
    else if (buf->mCaptured == BUFFER_ANCHOR)
      string.Format("%c: %s", letter, "Anchor image");
    else
      string.Format("%c: %s%s", letter, "Continuous, ", mModeNames[setUsed]);
    
  } else {
    // Otherwise say it's saved, or read in from file
    if (saveFlag < 0 && setUsed == MONTAGE_CONSET) 
      string.Format("%c: Saved to File, frame %d", letter, buf->mSecNumber);
    else if (saveFlag < 0) 
      string.Format("%c: Saved to File, sec. %d", letter, buf->mSecNumber);
    else if (buf->mSecNumber >= 0)
      string.Format("%c: Read from File, sec. %d", letter, buf->mSecNumber);
    else  
      string.Format("%c: From Other File, sec. %d", letter, -buf->mSecNumber-1);
  }
  mBufText[indStat]->SetWindowText(string);
}

BOOL EMstatusWindow::OnInitDialog() 
{
  CToolDlg::OnInitDialog();
  
  mBufferManager = mWinApp->mBufferManager;
  mImBufs = mWinApp->GetImBufs();

  mModeNames = mWinApp->GetModeNames();

  m_sbcChangeBuf.SetRange(0,100);
  m_sbcChangeBuf.SetPos(50);
  mInitialized = true;
  Update();

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void EMstatusWindow::OnCancelMode() 
{
  CToolDlg::OnCancelMode();
  
  
}
