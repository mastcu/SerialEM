// FilterControlDlg.cpp:  Has controls for working with the energy filter
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
#include "FilterControlDlg.h"
#include "CameraController.h"
#include "TSController.h"
#include "FilterTasks.h"
#include "Utilities\KGetOne.h"
#include ".\filtercontroldlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define DELTA_WIDTH   5.0f
#define DELTA_LOSS    1.0f
#define WIDTH_FORMAT  "%.0f"
#define LOSS_FORMAT   "%.1f"

/////////////////////////////////////////////////////////////////////////////
// CFilterControlDlg dialog


CFilterControlDlg::CFilterControlDlg(CWnd* pParent /*=NULL*/)
  : CToolDlg(CFilterControlDlg::IDD, pParent)
{
  //{{AFX_DATA_INIT(CFilterControlDlg)
  m_bAutoMag = FALSE;
  m_bAutoCamera = FALSE;
  m_bEFTEMMode = FALSE;
  m_bSlitIn = FALSE;
  m_strLoss = _T("");
  m_strWidth = _T("");
  m_strOnOff = _T("");
  m_bMatchPixel = FALSE;
  m_bMatchIntensity = FALSE;
  m_bDoMagShifts = FALSE;
  m_strMagShift = _T("");
  m_strOffset = _T("");
  m_bZeroLoss = FALSE;
  //}}AFX_DATA_INIT
  mLensEFTEM = false;
  mSlitOffset = 40.;
  mLastMagInLM = -1;
  mWarnedJeolLM = false;
}


void CFilterControlDlg::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CFilterControlDlg)
	DDX_Control(pDX, IDC_OFFSET_SLIT, m_butOffsetSlit);
	DDX_Control(pDX, IDC_STATMORE, m_statMore);
	DDX_Control(pDX, IDC_BUTMORE, m_butMore);
	DDX_Control(pDX, IDC_REFINE_ZLP, m_butRefineZLP);
  DDX_Control(pDX, IDC_STATOFFSET, m_statOffset);
  DDX_Control(pDX, IDC_ZEROLOSS, m_butZeroLoss);
  DDX_Control(pDX, IDC_STATMAGSHIFT, m_statMagShift);
  DDX_Control(pDX, IDC_DOMAGSHIFTS, m_butDoMagShifts);
  DDX_Control(pDX, IDC_MATCHINTENSITY, m_butMatchIntensity);
  DDX_Control(pDX, IDC_MATCHPIXEL, m_butMatchPixel);
  DDX_Control(pDX, IDC_STATONOFF, m_statOnOff);
  DDX_Control(pDX, IDC_SLITIN, m_butSlitIn);
  DDX_Control(pDX, IDC_EFTEMMODE, m_butEFTEMMode);
  DDX_Control(pDX, IDC_AUTOMAG, m_butAutoMag);
  DDX_Control(pDX, IDC_AUTOCAMERA, m_butAutoCamera);
  DDX_Control(pDX, IDC_SPINWIDTH, m_sbcWidth);
  DDX_Control(pDX, IDC_SPINLOSS, m_sbcLoss);
  DDX_Control(pDX, IDC_EDITWIDTH, m_editWidth);
  DDX_Control(pDX, IDC_EDITLOSS, m_editLoss);
  DDX_Check(pDX, IDC_AUTOMAG, m_bAutoMag);
  DDX_Check(pDX, IDC_AUTOCAMERA, m_bAutoCamera);
  DDX_Check(pDX, IDC_EFTEMMODE, m_bEFTEMMode);
  DDX_Check(pDX, IDC_SLITIN, m_bSlitIn);
  DDX_Text(pDX, IDC_EDITLOSS, m_strLoss);
  DDV_MaxChars(pDX, m_strLoss, 10);
  DDX_Text(pDX, IDC_EDITWIDTH, m_strWidth);
  DDV_MaxChars(pDX, m_strWidth, 10);
  DDX_Text(pDX, IDC_STATONOFF, m_strOnOff);
  DDX_Check(pDX, IDC_MATCHPIXEL, m_bMatchPixel);
  DDX_Check(pDX, IDC_MATCHINTENSITY, m_bMatchIntensity);
  DDX_Check(pDX, IDC_DOMAGSHIFTS, m_bDoMagShifts);
  DDX_Text(pDX, IDC_STATMAGSHIFT, m_strMagShift);
  DDX_Text(pDX, IDC_STATOFFSET, m_strOffset);
  DDX_Check(pDX, IDC_ZEROLOSS, m_bZeroLoss);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CFilterControlDlg, CToolDlg)
  //{{AFX_MSG_MAP(CFilterControlDlg)
  ON_EN_KILLFOCUS(IDC_EDITWIDTH, OnKillfocusEdits)
  ON_BN_CLICKED(IDC_AUTOCAMERA, OnOptionButton)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINWIDTH, OnDeltaposSpinwidth)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINLOSS, OnDeltaposSpinloss)
  ON_BN_CLICKED(IDC_EFTEMMODE, OnEftemmode)
  ON_BN_CLICKED(IDC_SLITIN, OnSlitin)
  ON_BN_CLICKED(IDC_ZLPALIGNED, OnZlpaligned)
  ON_BN_CLICKED(IDC_DOMAGSHIFTS, OnDomagshifts)
  ON_BN_CLICKED(IDC_ZEROLOSS, OnZeroloss)
	ON_BN_CLICKED(IDC_REFINE_ZLP, OnRefineZlp)
  ON_BN_CLICKED(IDC_AUTOMAG, OnOptionButton)
  ON_EN_KILLFOCUS(IDC_EDITLOSS, OnKillfocusEdits)
  ON_BN_CLICKED(IDC_MATCHPIXEL, OnOptionButton)
  ON_BN_CLICKED(IDC_MATCHINTENSITY, OnOptionButton)
	ON_BN_CLICKED(IDC_OFFSET_SLIT, OnOffsetSlit)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFilterControlDlg message handlers

void CFilterControlDlg::OnEftemmode() 
{
  mWinApp->RestoreViewFocus();
  UpdateData(true);
  mWinApp->SetEFTEMMode(m_bEFTEMMode);
}

void CFilterControlDlg::OnSlitin() 
{
  mWinApp->RestoreViewFocus();
  UpdateData(true);
  mParam->slitIn = m_bSlitIn;
  Update();
  ManageMagShift();
  mWinApp->mCamera->SetupFilter();
}


void CFilterControlDlg::OnOK()
{
  OnKillfocusEdits();
}

void CFilterControlDlg::OnKillfocusEdits() 
{
  float width, loss;
  mWinApp->RestoreViewFocus();
  UpdateData(true);
  width = (float)atof((LPCTSTR)m_strWidth);
  loss = (float)atof((LPCTSTR)m_strLoss);
  if (width < mParam->minWidth || width > mParam->maxWidth || loss < mParam->minLoss || 
    loss > mParam->maxLoss) {
    if (width < mParam->minWidth)
      width = mParam->minWidth;
    if (width > mParam->maxWidth)
      width = mParam->maxWidth;
    m_strWidth.Format(WIDTH_FORMAT, width);
    if (loss < mParam->minLoss)
      loss = mParam->minLoss;
    if (loss > mParam->maxLoss)
      loss = mParam->maxLoss;
    m_strLoss.Format(LOSS_FORMAT, loss);
    UpdateData(false);
  }
  mParam->energyLoss = loss;
  mParam->slitWidth = width;
  ManageMagShift();
  mWinApp->mCamera->SetupFilter();
}

void CFilterControlDlg::OnOptionButton() 
{
  mWinApp->RestoreViewFocus();
  UpdateData(true);
  mParam->autoCamera = m_bAutoCamera; 
  mParam->autoMag = m_bAutoMag; 
  mParam->matchPixel = m_bMatchPixel;
  mParam->matchIntensity = m_bMatchIntensity;
}

void CFilterControlDlg::OnDeltaposSpinwidth(NMHDR* pNMHDR, LRESULT* pResult) 
{
  mWinApp->RestoreViewFocus();
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  float newVal = mParam->slitWidth + pNMUpDown->iDelta * DELTA_WIDTH;
  if (newVal < mParam->minWidth)
    newVal = mParam->minWidth;
  if (newVal > mParam->maxWidth)
    newVal = mParam->maxWidth;
  if (newVal != mParam->slitWidth) {
    m_strWidth.Format(WIDTH_FORMAT, newVal);
    mParam->slitWidth = newVal;
    ManageMagShift();
    mWinApp->mCamera->SetupFilter();
  }
  *pResult = 1;
}

void CFilterControlDlg::OnDeltaposSpinloss(NMHDR* pNMHDR, LRESULT* pResult) 
{
  mWinApp->RestoreViewFocus();
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  float newVal = mParam->energyLoss + pNMUpDown->iDelta * DELTA_LOSS;
  if (newVal < mParam->minLoss)
    newVal = mParam->minLoss;
  if (newVal > mParam->maxLoss)
    newVal = mParam->maxLoss;
  if (newVal != mParam->energyLoss) {
    m_strLoss.Format(LOSS_FORMAT, newVal);
    mParam->energyLoss = newVal;
    ManageMagShift();
    mWinApp->mCamera->SetupFilter();
  }
  *pResult = 1;
}

BOOL CFilterControlDlg::OnInitDialog() 
{
  CRect rect, wndRect, magRect, clientRect;
  int leftOfs, leftPos, topOfs, topPos;

  CToolDlg::OnInitDialog();
  
  m_statOnOff.GetWindowRect(&rect);
  mFont.CreateFont((rect.Height() - 4), 0, 0, 0, FW_HEAVY,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, mBigFontName);
  m_statOnOff.SetFont(&mFont);
  mParam = mWinApp->GetFilterParams();

  m_sbcLoss.SetRange(0,100);
  m_sbcLoss.SetPos(50);
  m_sbcWidth.SetRange(0,100);
  m_sbcWidth.SetPos(50);

  // For Omega filter, move the mag shifts button over the auto mag button
  // and eliminate the option button
  if (mWinApp->mScope->GetHasOmegaFilter()) {
    GetClientRect(clientRect);
    GetWindowRect(wndRect);
    leftOfs = (wndRect.Width() - clientRect.Width()) / 2;
    topOfs = wndRect.Height() - clientRect.Height() - leftOfs;
    m_butAutoMag.GetWindowRect(magRect);
    leftPos = magRect.left - wndRect.left - leftOfs;
    topPos = magRect.top - wndRect.top - topOfs;
    m_butAutoMag.ShowWindow(SW_HIDE);
    m_butDoMagShifts.SetWindowPos(NULL, leftPos, topPos, 0, 0, 
      SWP_NOZORDER | SWP_NOSIZE);
    m_butMore.ShowWindow(SW_HIDE);
    m_statMore.ShowWindow(SW_HIDE);
    m_butEFTEMMode.ShowWindow(SW_HIDE);
    m_statOnOff.ShowWindow(SW_HIDE);
    if (mParam->positiveLossOnly)
      m_butOffsetSlit.ShowWindow(SW_SHOW);
  } else
    m_butAutoMag.ShowWindow(mWinApp->mScope->GetCanControlEFTEM() ? SW_SHOW : SW_HIDE);

  UpdateSettings(); 

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

// Trim off the option line if omega filter
int CFilterControlDlg::GetMidHeight()
{
  int height = CToolDlg::GetMidHeight();
  if (mWinApp->mScope->GetHasOmegaFilter())
    height -= 14;
  return height;
}

void CFilterControlDlg::Update()
{
  BOOL slitOK = true, lossOK = true;
  BOOL bEnable = !mWinApp->DoingTasks() && !mWinApp->GetSTEMMode();
  BOOL bOmega = mWinApp->mScope->GetHasOmegaFilter();
  if (mWinApp->mCamera)
    bEnable = bEnable && !mWinApp->mCamera->CameraBusy();
  if (mWinApp->mTSController && mWinApp->mScope && mWinApp->StartedTiltSeries() && 
    (!mWinApp->LowDoseMode() || mWinApp->mScope->GetLowDoseArea() == RECORD_CONSET)) {
    slitOK = !mWinApp->mTSController->GetTypeVaries(TS_VARY_SLIT);
    lossOK = !mWinApp->mTSController->GetTypeVaries(TS_VARY_ENERGY);
  }
  m_butEFTEMMode.EnableWindow(!mWinApp->DoingTasks() && !bOmega && 
    (mWinApp->mScope->GetCanControlEFTEM() || (JEOLscope && 
    mWinApp->mScope->FastMagIndex() < mWinApp->mScope->GetLowestMModeMagInd())));
  m_butDoMagShifts.EnableWindow(bEnable);
  m_butOffsetSlit.EnableWindow(bEnable && bOmega);
  ManageZLPAligned();
  bEnable = bEnable && m_bEFTEMMode;
  m_butRefineZLP.EnableWindow(bEnable);
  m_butSlitIn.EnableWindow(bEnable && slitOK && lossOK);
  m_editWidth.EnableWindow(bEnable && slitOK);
  m_sbcWidth.EnableWindow(bEnable && slitOK);
  m_butZeroLoss.EnableWindow(bEnable && m_bSlitIn && lossOK);
  bEnable = bEnable && m_bSlitIn && !m_bZeroLoss && lossOK;
  m_editLoss.EnableWindow(bEnable);
  m_sbcLoss.EnableWindow(bEnable);
  m_butAutoCamera.EnableWindow(!mWinApp->DoingComplexTasks() && 
    mWinApp->GetAdministrator() && !bOmega);
  bEnable = !bOmega && !mWinApp->DoingTasks() && mParam->firstRegularCamera >= 0;
  m_butMatchIntensity.EnableWindow(bEnable && !mWinApp->LowDoseMode());
  m_butMatchPixel.EnableWindow(bEnable);
  m_butAutoMag.EnableWindow(!bOmega);
}

void CFilterControlDlg::UpdateSettings()
{
  m_bAutoCamera = mParam->autoCamera;
  m_bAutoMag = mParam->autoMag;
  m_bMatchPixel = mParam->matchPixel;
  m_bMatchIntensity = mParam->matchIntensity;
  m_bSlitIn = mParam->slitIn;
  m_bZeroLoss = mParam->zeroLoss;
  m_bDoMagShifts = mParam->doMagShifts;
  m_strLoss.Format(LOSS_FORMAT, mParam->energyLoss);
  m_strWidth.Format(WIDTH_FORMAT, mParam->slitWidth);
  m_bEFTEMMode = mWinApp->GetFilterMode();
  if (m_bEFTEMMode)
    m_strOnOff = "ON";
  else
    m_strOnOff = "OFF";
  ManageMagShift();
  Update();
}

// Notification  of new mag and of potential change in EFTEM mode
// Store the EFTEM flag, manage enables, and if in EFTEM, compute a new mag shift
void CFilterControlDlg::NewMagUpdate(int inMag, BOOL EFTEM)
{
  int lowestM = mWinApp->mScope->GetLowestMModeMagInd();
  int inLM = inMag < lowestM ? 1 : 0;

  // Update the dialog to enable or disable the EFTEM button 
  if (JEOLscope && !mWinApp->mScope->GetHasOmegaFilter() && inLM != mLastMagInLM 
    && mWinApp->mScope->GetUseJeolGIFmodeCalls() <= 0) {
    Update();
    if (inLM && !mWarnedJeolLM) {
      mWarnedJeolLM = true;
      mWinApp->AppendToLog("\r\nWARNING: GIF MODE CANNOT BE DETECTED IN LOW MAG:\r\n"
        "   TOGGLE THE EFTEM MODE CHECKBOX WHEN YOU GO IN OR OUT OF GIF MODE IN LM\r\n");
    }
  }
  mLastMagInLM = inLM;
  mLensEFTEM = EFTEM;
  ManageZLPAligned();
  if (EFTEM) {

    // The first time into EFTEM, set the aligned mag  to the current mag
    if (!mParam->alignedMagInd) {
      mParam->alignedMagInd = inMag;
      mParam->alignedSlitWidth = mParam->slitWidth;
    }
    ManageMagShift();
    if (mParam->slitIn)
      mWinApp->mCamera->SetupFilter();
  }
}

// If user says ZLP was aligned, then set the reference mag for shifts, take care
// of finding shift, and set filter if slit is in
// Do not try to set filter if program detects a ZLP align
void CFilterControlDlg::OnZlpaligned() 
{
  AdjustForZLPAlign();
  if (mParam->slitIn)
     mWinApp->mCamera->SetupFilter();
}

void CFilterControlDlg::AdjustForZLPAlign()
{
  mParam->alignedMagInd = mWinApp->mScope->GetMagIndex();
  mParam->alignedSlitWidth = mParam->slitWidth;
  mParam->refineZLPOffset = 0.;
  mParam->alignZLPTimeStamp = 0.001 * GetTickCount();
  mParam->cumulNonRunTime = 0;
  mParam->usedOldAlign = false;
  mWinApp->mDocWnd->SetShortTermNotSaved();
  ManageMagShift();
}

// If Do mag shifts changes, manage enables and adjust filter if necessary
void CFilterControlDlg::OnDomagshifts() 
{
  mWinApp->RestoreViewFocus();
  UpdateData(true);
  mParam->doMagShifts = m_bDoMagShifts;
  ManageZLPAligned();
  ManageMagShift();
  if (mParam->slitIn)
    mWinApp->mCamera->SetupFilter();
}

// Compute the mag shift for the current magnification, and display it
void CFilterControlDlg::ManageMagShift()
{
  float shift = 0.;
  float curShift, refShift;
  if (mWinApp->GetSTEMMode())
    return;
  int curMag = mWinApp->mScope->FastMagIndex();
  refShift = LookUpShift(mParam->alignedMagInd);
  curShift = LookUpShift(curMag);
  if (curShift > -900. && refShift > -900.)
    shift = curShift - refShift;
  
  // Include the adjustment for slit width in the mag shift, but always apply 
  // refine offset
  if (mParam->alignedSlitWidth && mParam->adjustForSlitWidth)
    shift -= (mParam->slitWidth - mParam->alignedSlitWidth) / 2.f;
  mParam->currentMagShift = shift;
  shift += mParam->refineZLPOffset;
  double loss = LossToApply();
  m_strMagShift.Format("Adjustment = "LOSS_FORMAT, shift);
  m_strOffset.Format("Net %s = "LOSS_FORMAT, 
    mWinApp->mScope->GetHasOmegaFilter() ? "Shift" : "Offset", loss);
  UpdateData(false);
}

// Find shift of nearest mag that has a defined shift
float CFilterControlDlg::LookUpShift(int inMag)
{
  int iDel, iDir, iMag;
  for (iDel = 0; iDel < MAX_MAGS; iDel++) {
    for (iDir = -1; iDir <= 1; iDir += 2) {
      iMag = inMag + iDir * iDel;
      if (iMag < 1 || iMag >= MAX_MAGS)
        continue;
      if (mParam->magShifts[iMag] > -900.)
        return mParam->magShifts[iMag];
    }
  }
  return -999.;
}

// Compute the actual energy offset to apply.  If slit is in, always apply the refine
// offset; if not zero loss, apply selected loss; if mag shift enabled, add that
double CFilterControlDlg::LossToApply()
{
  double loss = mParam->refineZLPOffset;
  if (!mParam->slitIn)
    return 0;
  if (!mParam->zeroLoss)
    loss += mParam->energyLoss;
  if (mParam->doMagShifts)
    loss += mParam->currentMagShift;
  return loss;
}

// Check a loss before modifying parameters
BOOL CFilterControlDlg::LossOutOfRange(double nominal, double & netLoss)
{
  float saveLoss = mParam->energyLoss;
  BOOL saveZero = mParam->zeroLoss;
  mParam->zeroLoss = false;
  mParam->energyLoss = (float)nominal;
  netLoss = LossToApply();
  mParam->zeroLoss = saveZero;
  mParam->energyLoss = saveLoss;
  return ((netLoss < mParam->minLoss) || (mParam->positiveLossOnly && netLoss < 0) ||
    netLoss > mParam->maxLoss);
}

// Manage the status of the ZLP aligned button and mag shift output
void CFilterControlDlg::ManageZLPAligned()
{
  BOOL bEnable = !mWinApp->DoingTasks();
  BOOL STEM = mWinApp->GetSTEMMode();
  if (mWinApp->mCamera)
    bEnable = bEnable && !mWinApp->mCamera->CameraBusy();
//  m_butZLPAligned.EnableWindow(bEnable && mLensEFTEM && m_bDoMagShifts);
  m_statMagShift.EnableWindow(!STEM && m_bSlitIn && mLensEFTEM);
  m_statOffset.EnableWindow(!STEM && mLensEFTEM);
}

void CFilterControlDlg::OnZeroloss() 
{
  mWinApp->RestoreViewFocus();
  UpdateData(true);
  mParam->zeroLoss = m_bZeroLoss;
  ManageMagShift();
  Update();
  mWinApp->mCamera->SetupFilter();
}

// Entry to call the align ZLP function
void CFilterControlDlg::OnRefineZlp() 
{
  mWinApp->RestoreViewFocus();
  mWinApp->mFilterTasks->RefineZLP(true);
}

// For JEOL, need to offset slit and energy loss from zero
void CFilterControlDlg::OnOffsetSlit() 
{
  if (!JEOLscope)
    return;
  mWinApp->RestoreViewFocus();
  int spectMode = mWinApp->mScope->GetSpectroscopyMode();
  FilterParams paramSave = *mParam;
  if (!KGetOneFloat("The slit must be aligned to a positive energy shift for Refine ZLP"
    " and","mag-dependent shifts to work.  Enter energy loss at which to center slit:",
    mSlitOffset, 0))
    return;

  // Set up for offset imposed by adjustment, and go to spectroscopy mode.
  mParam->slitIn = true;
  mParam->zeroLoss = true;
  mParam->doMagShifts = false;
  mParam->refineZLPOffset = mSlitOffset;
  mWinApp->mCamera->SetupFilter();
  UpdateSettings();
  if (spectMode != 1)
    mWinApp->mScope->SetSpectroscopyMode(1);
  AfxMessageBox("Center the zero-loss peak in the slit with this energy shift.\n\n"
    "Do not change the energy shift\n\nWhen you are done, press OK then try Refine ZLP",
    MB_ICONINFORMATION | MB_OK);

  // Restore imaging mode if it was on
  if (spectMode != 1)
    mWinApp->mScope->SetSpectroscopyMode(0);
 
  *mParam = paramSave;
  AdjustForZLPAlign();
  mParam->refineZLPOffset = mSlitOffset;
  mWinApp->mCamera->SetupFilter();
  UpdateSettings();
}

