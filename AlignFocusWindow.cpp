// AlignFocusWindow.cpp:  Has image alignment and autofocusing controls
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include ".\AlignFocusWindow.h"
#include "EMscope.h"
#include "ShiftManager.h"
#include "ShiftCalibrator.h"
#include "FocusManager.h"
#include "EMbufferManager.h"
#include "CameraController.h"
#include "ComplexTasks.h"
#include "TSController.h"
#include "Utilities\KGetOne.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static int sIdTable[] = {IDC_BUTOPEN, IDC_BUTFLOATDOCK, IDC_STATTOPLINE, IDC_BUTHELP,
PANEL_END,
IDC_BUTALIGN, IDC_BUTFOCUS, IDC_BUT_TO_MARKER, IDC_BUTCLEARALIGN,
IDC_BUTRESETSHIFT, IDC_BUTMORE, IDC_STATMORE, IDC_STATDEFTARGET, PANEL_END,
IDC_MOUSESTAGE, IDC_BUTSTAGETHRESH, IDC_CENTERONAXIS, IDC_TRIMBORDERS,
IDC_APPLYISOFFSET, IDC_CORRECT_BACKLASH, IDC_SET_TRIM_FRAC, PANEL_END, TABLE_END};

static int sTopTable[sizeof(sIdTable) / sizeof(int)];
static int sLeftTable[sizeof(sIdTable) / sizeof(int)];
static int sHeightTable[sizeof(sIdTable) / sizeof(int)];

/////////////////////////////////////////////////////////////////////////////
// CAlignFocusWindow dialog


CAlignFocusWindow::CAlignFocusWindow(CWnd* pParent /*=NULL*/)
  : CToolDlg(CAlignFocusWindow::IDD, pParent)
  , m_strDefTarget(_T(""))
  , m_bApplyISoffset(FALSE)
  , m_bCorrectBacklash(FALSE)
{
  SEMBuildTime(__DATE__, __TIME__);
  //{{AFX_DATA_INIT(CAlignFocusWindow)
	m_bMouseStage = FALSE;
	m_bCenterOnAxis = FALSE;
	m_bTrimBorders = FALSE;
	//}}AFX_DATA_INIT
  mInitialized = false;
}


void CAlignFocusWindow::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CAlignFocusWindow)
  DDX_Control(pDX, IDC_TRIMBORDERS, m_butTrimBorders);
  DDX_Control(pDX, IDC_CENTERONAXIS, m_butCenterOnAxis);
  DDX_Control(pDX, IDC_BUTRESETSHIFT, m_butResetShift);
  DDX_Control(pDX, IDC_BUTFOCUS, m_butFocus);
  DDX_Control(pDX, IDC_BUTCLEARALIGN, m_butClearAlign);
  DDX_Control(pDX, IDC_BUTALIGN, m_butAlign);
  DDX_Check(pDX, IDC_MOUSESTAGE, m_bMouseStage);
  DDX_Check(pDX, IDC_CENTERONAXIS, m_bCenterOnAxis);
  DDX_Check(pDX, IDC_TRIMBORDERS, m_bTrimBorders);
  DDX_Text(pDX, IDC_STATDEFTARGET, m_strDefTarget);
  DDX_Control(pDX, IDC_APPLYISOFFSET, m_butApplyISoffset);
  DDX_Check(pDX, IDC_APPLYISOFFSET, m_bApplyISoffset);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_MOUSESTAGE, m_butMouseStage);
  DDX_Control(pDX, IDC_STATDEFTARGET, m_statDefTarget);
  DDX_Check(pDX, IDC_CORRECT_BACKLASH, m_bCorrectBacklash);
  DDX_Control(pDX, IDC_BUT_TO_MARKER, m_butToMarker);
}


BEGIN_MESSAGE_MAP(CAlignFocusWindow, CToolDlg)
  //{{AFX_MSG_MAP(CAlignFocusWindow)
  ON_BN_CLICKED(IDC_BUTALIGN, OnButalign)
  ON_BN_CLICKED(IDC_BUTCLEARALIGN, OnButclearalign)
  ON_BN_CLICKED(IDC_BUTRESETSHIFT, OnButresetshift)
  ON_WM_CANCELMODE()
  ON_BN_CLICKED(IDC_BUTFOCUS, OnButfocus)
	ON_BN_CLICKED(IDC_MOUSESTAGE, OnMousestage)
	ON_BN_CLICKED(IDC_BUTSTAGETHRESH, OnButstagethresh)
	ON_BN_CLICKED(IDC_CENTERONAXIS, OnCenteronaxis)
	ON_BN_CLICKED(IDC_TRIMBORDERS, OnTrimborders)
	ON_BN_CLICKED(IDC_SET_TRIM_FRAC, OnSetTrimFrac)
  ON_BN_CLICKED(IDC_APPLYISOFFSET, OnApplyISoffset)
	//}}AFX_MSG_MAP
  ON_BN_CLICKED(IDC_CORRECT_BACKLASH, OnCorrectBacklash)
  ON_BN_CLICKED(IDC_BUT_TO_MARKER, OnButToMarker)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAlignFocusWindow message handlers

void CAlignFocusWindow::OnButalign() 
{
  mWinApp->mMainView->SetFocus(); // Doesn't work?
  if (!mWinApp->DoingTasks())
    mWinApp->mShiftManager->AutoAlign(0, 0);
}


void CAlignFocusWindow::OnButToMarker()
{
  mWinApp->RestoreViewFocus();
  if (!mWinApp->DoingTasks() || mWinApp->GetJustNavAcquireOpen())
    mWinApp->mShiftManager->AlignmentShiftToMarker(GetAsyncKeyState(VK_SHIFT) / 2 != 0);
}

void CAlignFocusWindow::OnButclearalign()
{
  mWinApp->RestoreViewFocus();
  EMimageBuffer *activeImBuf = mWinApp->mMainView->GetActiveImBuf();
  mWinApp->mShiftManager->SetAlignShifts(0., 0., false, activeImBuf,
    !mWinApp->mScope->GetNoScope());
}

void CAlignFocusWindow::OnButresetshift() 
{
  mWinApp->RestoreViewFocus();
  mWinApp->mShiftManager->ResetImageShift(true, false);
}

void CAlignFocusWindow::OnButfocus() 
{
  mWinApp->RestoreViewFocus();
  if ((!mWinApp->DoingTasks() || mWinApp->GetJustNavAcquireOpen() ) && 
    mWinApp->mFocusManager->FocusReady())
    mWinApp->mFocusManager->AutoFocusStart(1);
}

void CAlignFocusWindow::OnMousestage() 
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
  mWinApp->mShiftManager->SetMouseMoveStage(m_bMouseStage);
}

void CAlignFocusWindow::OnButstagethresh() 
{
  float thresh = mWinApp->mShiftManager->GetMouseStageThresh();	
  if (!KGetOneFloat("Threshold fraction of camera field for moving stage instead of "
    "doing image shift:", thresh, 2)) {
    mWinApp->RestoreViewFocus();  
    return;
  }
  if (thresh < 0.)
    thresh = 0.;
  if (thresh > 1.)
    thresh = 1.;
  mWinApp->mShiftManager->SetMouseStageThresh(thresh);

  thresh = mWinApp->mShiftManager->GetMouseStageAbsThresh();
  if (KGetOneFloat("Threshold number of microns for moving stage instead of "
    "doing image shift:", thresh, 1)) {
    if (thresh < 0.)
      thresh = 0.;
    mWinApp->mShiftManager->SetMouseStageAbsThresh(thresh);
  }
  mWinApp->RestoreViewFocus();  
}

void CAlignFocusWindow::OnSetTrimFrac() 
{
  float frac = mWinApp->mShiftManager->GetTrimFrac();
  if (!KGetOneFloat("This value will not be stored between SerialEM sessions.", 
    "Fraction to trim off each side of image before aligning (range 0.04-0.4):", 
    frac, 2)) {
    mWinApp->RestoreViewFocus();  
    return;
  }
  if (frac < 0.04)
    frac = 0.04f;
  if (frac > 0.4)
    frac = 0.4f;
  mWinApp->mShiftManager->SetTrimFrac(frac);
  mWinApp->RestoreViewFocus();  
}


void CAlignFocusWindow::OnCorrectBacklash()
{
 	UpdateData(true);
  mWinApp->mShiftManager->SetBacklashMouseAndISR(m_bCorrectBacklash);
  mWinApp->RestoreViewFocus();  
}

void CAlignFocusWindow::OnCenteronaxis() 
{
 	UpdateData(true);
  mWinApp->mScope->SetShiftToTiltAxis(m_bCenterOnAxis);
  mWinApp->RestoreViewFocus();
}

void CAlignFocusWindow::OnApplyISoffset()
{
  UpdateData(true);
  mWinApp->mScope->SetApplyISoffset(m_bApplyISoffset);
  mWinApp->RestoreViewFocus();
}

void CAlignFocusWindow::OnTrimborders() 
{
 	UpdateData(true);
  mWinApp->mShiftManager->SetTrimDarkBorders(m_bTrimBorders);
  mWinApp->RestoreViewFocus();
}

BOOL CAlignFocusWindow::OnInitDialog() 
{
  CToolDlg::OnInitDialog();
  mInitialized = true;
  UpdateSettings(); 
  SetupPanels(sIdTable, sLeftTable, sTopTable, sHeightTable);
  mInitialized = true;
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}


void CAlignFocusWindow::UpdateSettings()
{
  if (!mInitialized)
    return;
  m_bMouseStage = mWinApp->mShiftManager->GetMouseMoveStage();
  m_bCorrectBacklash = mWinApp->mShiftManager->GetBacklashMouseAndISR();
  m_bCenterOnAxis = mWinApp->mScope->GetShiftToTiltAxis();
  m_bApplyISoffset = mWinApp->mScope->GetApplyISoffset();
  m_bTrimBorders = mWinApp->mShiftManager->GetTrimDarkBorders();
  m_strDefTarget.Format("Def. target = %.2f um", 
    mWinApp->mFocusManager->GetTargetDefocus());
  UpdateData(false);
  Update();
}

void CAlignFocusWindow::UpdateHiding(void)
{
  SetOpenClosed(GetState());
}

void CAlignFocusWindow::OnCancelMode() 
{
  CToolDlg::OnCancelMode();
}

void CAlignFocusWindow::Update()
{
  // Disable the action buttons if doing tasks
  BOOL bEnable;
  BOOL legacyLD = false;
  BOOL bTasks = mWinApp->DoingTasks();
  BOOL justNavAcq = mWinApp->GetJustNavAcquireOpen();
  CString alignText = "Align to ";
  char letter;
  EMimageBuffer *imBufs = mWinApp->GetImBufs();
  MagTable *magTab = mWinApp->GetMagTable();

  // Set up the align to button with the actual buffer for the image in A in low dose
  letter = 'A' + mWinApp->mBufferManager->AutoalignBufferIndex();
  if (mWinApp->LowDoseMode() && (!imBufs[0].mImage || legacyLD)) {
    letter = 'A' + mWinApp->mBufferManager->AutoalignBasicIndex();
    alignText += letter;
    letter++;
    alignText += "/";
  }
  alignText += letter;
  SetDlgItemText(IDC_BUTALIGN, alignText);

  // Autoalign enabled if images in both buffers
  bEnable = imBufs[0].mImage && 
    imBufs[mWinApp->mBufferManager->AutoalignBufferIndex()].mImage;
  m_butAlign.EnableWindow(bEnable && (!bTasks || justNavAcq));

  m_butToMarker.EnableWindow(imBufs[0].mImage && (!bTasks || justNavAcq));

  UpdateAutofocus(-1);
  m_statDefTarget.EnableWindow(!mWinApp->GetSTEMMode());

  // Clear align requires an active image
  imBufs = mWinApp->mMainView->GetActiveImBuf();
  m_butClearAlign.EnableWindow(imBufs->mImage && (!bTasks || justNavAcq));

  // Reset shift shouldn't be done if camera busy or tilt series in action
  bEnable = !bTasks && !mWinApp->StartedTiltSeries();
  if (mWinApp->mCamera)
    bEnable = (!mWinApp->mCamera->CameraBusy() || 
    mWinApp->mCamera->DoingContinuousAcquire()) && !bTasks;
  m_butResetShift.EnableWindow(bEnable && 
    (!mWinApp->mScope || !mWinApp->mScope->GetMovingStage()));

  // Same for shift to tilt axis
  m_butCenterOnAxis.EnableWindow(bEnable);

  // For image offset button, use same criteria then enable only if non-zero cal exists
  if (bEnable) {
    bEnable = false;
    for (int i = 0; !bEnable && i < MAX_MAGS; i++)
      if (magTab[i].calOffsetISX[0] || magTab[i].calOffsetISY[0] ||
        magTab[i].calOffsetISX[1] || magTab[i].calOffsetISY[1])
          bEnable = true;

    // If there are none and button is to be disabled, turn off function
    if (!bEnable && m_bApplyISoffset) {
      m_bApplyISoffset = false;
      UpdateData(false);
      mWinApp->mScope->SetApplyISoffset(m_bApplyISoffset);
    }
  }
  m_butApplyISoffset.EnableWindow(bEnable);

  // Mouse moving of stage disabled in calibrating IS offset
  m_butMouseStage.EnableWindow(!mWinApp->mShiftCalibrator->CalibratingOffset());
}

// Update the autofocus button: it is the only thing here that depends on scope state
void CAlignFocusWindow::UpdateAutofocus(int magInd)
{
  // Autofocus requires focus ready
  m_butFocus.EnableWindow(mWinApp->mFocusManager->FocusReady(magInd) && 
    (!mWinApp->DoingTasks() || mWinApp->GetJustNavAcquireOpen()) && 
    (!mWinApp->mScope || !mWinApp->mScope->GetMovingStage()));
}

