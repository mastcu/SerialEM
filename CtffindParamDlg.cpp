// CtffindParamDlg.cpp:      Dialog for ctffind parameters
//
// Copyright (C) 2019-2026 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMView.h"
#include "CtffindParamDlg.h"
#include "ProcessImage.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif


// CCtffindParamDlg dialog

CCtffindParamDlg::CCtffindParamDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_CTFFINDDLG, pParent)
  , m_bSlowSearch(FALSE)
  , m_fCustomMaxRes(0)
  , m_iMinPhase(0)
  , m_iMaxPhase(0)
  , m_bFindPhase(FALSE)
  , m_bFixAstigForPhase(FALSE)
  , m_bExtraStats(FALSE)
  , m_bDrawRings(FALSE)
  , m_fPhaseMinRes(0)
  , m_strDfltMaxRes(_T(""))
  , m_fFixedPhase(0)
  , m_bPlotterFitOnClick(FALSE)
  , m_bPlotterForTuning(FALSE)
{
  mNonModal = true;
}

CCtffindParamDlg::~CCtffindParamDlg()
{
}

void CCtffindParamDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Check(pDX, IDC_CHECK_SLOW_SEARCH, m_bSlowSearch);
  DDX_MM_FLOAT(pDX, IDC_EDIT_MAX_FIT_RES, m_fCustomMaxRes, 0, 100,
    "Highest resolution to fit");
  DDX_MM_INT(pDX, IDC_EDIT_MIN_PHASE, m_iMinPhase, -30, 180, "Lower phase limit");
  DDX_MM_INT(pDX, IDC_EDIT_MAX_PHASE, m_iMaxPhase, 0, 200, "Upper phase limit");
  DDX_Check(pDX, IDC_CHECK_FIND_PHASE, m_bFindPhase);
  DDX_Check(pDX, IDC_CHECK_FIX_ASTIG_FOR_PHASE, m_bFixAstigForPhase);
  DDX_Check(pDX, IDC_CHECK_EXTRA_STATS, m_bExtraStats);
  DDX_Check(pDX, IDC_CHECK_DRAW_RINGS, m_bDrawRings);
  DDX_MM_FLOAT(pDX, IDC_EDIT_PHASE_MIN_RES, m_fPhaseMinRes, 0, 100,
    "Lowest resolution when fitting with phase");
  DDX_Text(pDX, IDC_STAT_DEFAULT_MAX_RES, m_strDfltMaxRes);
  DDX_MM_FLOAT(pDX, IDC_EDIT_FIXED_PHASE, m_fFixedPhase, -20, 200, "Fixed phase shift");
  DDX_Check(pDX, IDC_CHECK_FIT_ON_CLICK, m_bPlotterFitOnClick);
  DDX_Check(pDX, IDC_CHECK_FOR_TUNING, m_bPlotterForTuning);
}


BEGIN_MESSAGE_MAP(CCtffindParamDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_CHECK_DRAW_RINGS, OnCheckDrawRings)
  ON_BN_CLICKED(IDC_CHECK_SLOW_SEARCH, OnCheckSlowSearch)
  ON_EN_KILLFOCUS(IDC_EDIT_MAX_FIT_RES, OnKillfocusEditMaxFitRes)
  ON_EN_KILLFOCUS(IDC_EDIT_PHASE_MIN_RES, OnKillfocusEditPhaseMinRes)
  ON_BN_CLICKED(IDC_CHECK_FIND_PHASE, OnCheckFindPhase)
  ON_BN_CLICKED(IDC_CHECK_FIX_ASTIG_FOR_PHASE, OnCheckFixAstigForPhase)
  ON_BN_CLICKED(IDC_CHECK_EXTRA_STATS, OnCheckExtraStats)
  ON_EN_KILLFOCUS(IDC_EDIT_MIN_PHASE, OnEnKillfocusEditMinPhase)
  ON_EN_KILLFOCUS(IDC_EDIT_MAX_PHASE, OnEnKillfocusEditMaxPhase)
  ON_EN_KILLFOCUS(IDC_EDIT_FIXED_PHASE, OnKillfocusEditFixedPhase)
  ON_BN_CLICKED(IDC_CHECK_FIT_ON_CLICK, OnCheckFitOnClick)
  ON_BN_CLICKED(IDC_CHECK_FOR_TUNING, OnCheckForTuning)
END_MESSAGE_MAP()


// CCtffindParamDlg message handlers

// Initialize dialog
BOOL CCtffindParamDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  UpdateSettings();
  ManageForCtfplotter();
  SetDefID(45678);    // Disable OK from being default button
  return TRUE;
}

// Load the variables for initial or changed settings
void CCtffindParamDlg::UpdateSettings()
{
  mProcessImage = mWinApp->mProcessImage;
  m_bPlotterFitOnClick = mProcessImage->GetClickUseCtfplotter();
  m_bPlotterForTuning = mProcessImage->GetTuneUseCtfplotter();
  m_bSlowSearch = mProcessImage->GetSlowerCtfFit();
  m_bExtraStats = mProcessImage->GetExtraCtfStats();
  m_fCustomMaxRes = mProcessImage->GetUserMaxCtfFitRes();
  m_bDrawRings = mProcessImage->GetDrawExtraCtfRings() != 0;
  m_fPhaseMinRes = mProcessImage->GetMinCtfFitResIfPhase();
  m_bFindPhase = mProcessImage->GetCtfFindPhaseOnClick();
  m_bFixAstigForPhase = mProcessImage->GetCtfFixAstigForPhase();
  m_iMinPhase = mProcessImage->GetCtfMinPhase();
  m_iMaxPhase = m_bPlotterFitOnClick ? mProcessImage->GetCtfExpectedPhase() :
    mProcessImage->GetCtfMaxPhase();
  m_fFixedPhase = (float)(mProcessImage->GetPlatePhase() / DTOR);
  m_strDfltMaxRes.Format("(0 for default = %.1f A; applies to Ctffind in script)",
    mProcessImage->GetDefaultMaxCtfFitRes());
  ManageForCtfplotter();
  UpdateData(false);
}

void CCtffindParamDlg::ManageForCtfplotter()
{
  EnableDlgItem(IDC_CHECK_SLOW_SEARCH, !m_bPlotterFitOnClick);
  EnableDlgItem(IDC_CHECK_EXTRA_STATS, !m_bPlotterFitOnClick);
  SetDlgItemText(IDC_CHECK_DRAW_RINGS, m_bPlotterFitOnClick ?
    "Draw rings to end of fitting range" : "Draw rings to computed resolution");
  SetDlgItemText(IDC_CHECK_FIND_PHASE, m_bPlotterFitOnClick ?
    "Find phase with expected phase" : "Find phase with search limits");
  ShowDlgItem(IDC_STAT_TO_MAX, !m_bPlotterFitOnClick);
  ShowDlgItem(IDC_EDIT_MIN_PHASE, !m_bPlotterFitOnClick);
  EnableDlgItem(IDC_EDIT_MIN_PHASE, m_bFindPhase);
  EnableDlgItem(IDC_EDIT_MAX_PHASE, m_bFindPhase);
  EnableDlgItem(IDC_STAT_CUSTOM_HIGH_RES, !m_bPlotterFitOnClick);
  EnableDlgItem(IDC_EDIT_MAX_FIT_RES, !m_bPlotterFitOnClick);
  EnableDlgItem(IDC_STAT_ANGSTROM1, !m_bPlotterFitOnClick);
  EnableDlgItem(IDC_STAT_DEFAULT_MAX_RES, !m_bPlotterFitOnClick);
}

void CCtffindParamDlg::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

// OK, Cancel, handling returns in text fields
void CCtffindParamDlg::OnOK()
{
  SetFocus();
  UPDATE_DATA_TRUE;
  mProcessImage->GetCtffindPlacement();
  mProcessImage->mCtffindParamDlg = NULL;
  DestroyWindow();
}

void CCtffindParamDlg::OnCancel()
{
  mProcessImage->GetCtffindPlacement();
  mProcessImage->mCtffindParamDlg = NULL;
  DestroyWindow();
}

BOOL CCtffindParamDlg::PreTranslateMessage(MSG* pMsg)
{
  //PrintfToLog("message %d  keydown %d  param %d return %d", pMsg->message, WM_KEYDOWN, pMsg->wParam, VK_RETURN);
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

// Use ctfplotter for click fits
void CCtffindParamDlg::OnCheckFitOnClick()
{
  UPDATE_DATA_TRUE;
  if (m_bPlotterFitOnClick) {
    mProcessImage->SetCtfMaxPhase(m_iMaxPhase);
    m_iMaxPhase = mProcessImage->GetCtfExpectedPhase();
  } else {
    mProcessImage->SetCtfExpectedPhase(m_iMaxPhase);
    m_iMaxPhase = mProcessImage->GetCtfMaxPhase();
  }
  UpdateData(false);
  mProcessImage->SetClickUseCtfplotter(m_bPlotterFitOnClick);
  ManageForCtfplotter();
  mWinApp->RestoreViewFocus();
  RefitToCurrentFFT();
}

// Use Ctfplotter for tuning
void CCtffindParamDlg::OnCheckForTuning()
{
  UPDATE_DATA_TRUE;
  mProcessImage->SetTuneUseCtfplotter(m_bPlotterForTuning);
  mWinApp->RestoreViewFocus();
}

// Draw extra rings
void CCtffindParamDlg::OnCheckDrawRings()
{
  UPDATE_DATA_TRUE;
  mProcessImage->SetDrawExtraCtfRings(m_bDrawRings ? 1 : 0);
  mWinApp->RestoreViewFocus();
}

// Slower search
void CCtffindParamDlg::OnCheckSlowSearch()
{
  UPDATE_DATA_TRUE;
  mProcessImage->SetSlowerCtfFit(m_bSlowSearch);
  RefitToCurrentFFT();
}

// Max res for fitting, limited to 3 A
void CCtffindParamDlg::OnKillfocusEditMaxFitRes()
{
  UPDATE_DATA_TRUE;
  if (m_fCustomMaxRes && m_fCustomMaxRes < 3.) {
    m_fCustomMaxRes = 3.;
    UpdateData(false);
  }
  mProcessImage->SetUserMaxCtfFitRes(m_fCustomMaxRes);
  RefitToCurrentFFT();
}

// Minimum res for fitting when phase is involved
void CCtffindParamDlg::OnKillfocusEditPhaseMinRes()
{
  UPDATE_DATA_TRUE;
  mProcessImage->SetMinCtfFitResIfPhase(m_fPhaseMinRes);
  RefitToCurrentFFT();
}

// Find phase
void CCtffindParamDlg::OnCheckFindPhase()
{
  UPDATE_DATA_TRUE;
  mProcessImage->SetCtfFindPhaseOnClick(m_bFindPhase);
  ManageForCtfplotter();
  RefitToCurrentFFT();
}

// Fix astigmatism when finding phase
void CCtffindParamDlg::OnCheckFixAstigForPhase()
{
  UPDATE_DATA_TRUE;
  mProcessImage->SetCtfFixAstigForPhase(m_bFixAstigForPhase);
  RefitToCurrentFFT();
}

// Get extra stats for res fit to
void CCtffindParamDlg::OnCheckExtraStats()
{
  UPDATE_DATA_TRUE;
  mProcessImage->SetExtraCtfStats(m_bExtraStats);
  RefitToCurrentFFT();
}

// Fixed phase
void CCtffindParamDlg::OnKillfocusEditFixedPhase()
{
  UPDATE_DATA_TRUE;
  mProcessImage->SetPlatePhase((float)DTOR * m_fFixedPhase);
  RefitToCurrentFFT();
}

// Min and max phase for fitting
void CCtffindParamDlg::OnEnKillfocusEditMinPhase()
{
  UPDATE_DATA_TRUE;
  if (m_iMinPhase > m_iMaxPhase) {
    m_iMinPhase = m_iMaxPhase - 1;
    UpdateData(false);
  }
  mProcessImage->SetCtfMinPhase(m_iMinPhase);
  RefitToCurrentFFT();
}

void CCtffindParamDlg::OnEnKillfocusEditMaxPhase()
{
  UPDATE_DATA_TRUE;
  if (m_bPlotterFitOnClick)
    mProcessImage->SetCtfExpectedPhase(m_iMaxPhase);
  else {
    if (m_iMinPhase > m_iMaxPhase) {
      m_iMinPhase = m_iMaxPhase - 1;
      UpdateData(false);
    }
    mProcessImage->SetCtfMaxPhase(m_iMaxPhase);
  }
  RefitToCurrentFFT();
}

// Any change should call this to redo the fit if there is one
void CCtffindParamDlg::RefitToCurrentFFT()
{
  EMimageBuffer *imBuf;
  CSerialEMView *view;
  FloatVec radii;
  double defocus;
  CString lenstr;
  int index;
  mWinApp->RestoreViewFocus();

  // Get the buffer with the FFT
  if (mWinApp->mFFTView) {
    view = mWinApp->mFFTView;
    index = mWinApp->GetImBufIndex(true);
    imBuf = mWinApp->GetFFTBufs() + index;
  } else {
    view = mWinApp->mMainView;
    index = mWinApp->GetImBufIndex(false);
    imBuf = mWinApp->GetImBufs() + index;
  }
  if (imBuf->mCtfFocus1 <= 0 || !imBuf->mHasUserPt)
    return;
  if (!mProcessImage->GetFFTZeroRadiiAndDefocus(imBuf, &radii, defocus))
    return;
  index = view->FitCtfAtMarkedPoint(imBuf, lenstr, defocus, radii);
  if (index < 0)
    return;
  if (mProcessImage->GetPlatePhase() > 0.001 || index > 0)
    lenstr += ": PP";
  mWinApp->SetStatusText(MEDIUM_PANE, lenstr);
  view->DrawImage();
}
