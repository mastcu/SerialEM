// CtffindParamDlg.cpp:      Dialog for ctffind parameters
//
// Copyright (C) 2019 by  the Regents of the University of
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
  DDX_Text(pDX, IDC_EDIT_MAX_FIT_RES, m_fCustomMaxRes);
  MinMaxFloat(IDC_EDIT_MAX_FIT_RES, m_fCustomMaxRes, 0, 100, "Highest resolution to fit");
  DDX_Text(pDX, IDC_EDIT_MIN_PHASE, m_iMinPhase);
  MinMaxInt(IDC_EDIT_MIN_PHASE, m_iMinPhase, -30, 180, "Lower phase limit");
  DDX_Text(pDX, IDC_EDIT_MAX_PHASE, m_iMaxPhase);
  MinMaxInt(IDC_EDIT_MAX_PHASE, m_iMaxPhase, 0, 200, "Upper phase limit");
  DDX_Check(pDX, IDC_CHECK_FIND_PHASE, m_bFindPhase);
  DDX_Check(pDX, IDC_CHECK_FIX_ASTIG_FOR_PHASE, m_bFixAstigForPhase);
  DDX_Check(pDX, IDC_CHECK_EXTRA_STATS, m_bExtraStats);
  DDX_Check(pDX, IDC_CHECK_DRAW_RINGS, m_bDrawRings);
  DDX_Text(pDX, IDC_EDIT_PHASE_MIN_RES, m_fPhaseMinRes);
  MinMaxFloat(IDC_EDIT_PHASE_MIN_RES, m_fPhaseMinRes, 0, 100, 
    "Lowest resolution when fitting with phase");
  DDX_Text(pDX, IDC_STAT_DEFAULT_MAX_RES, m_strDfltMaxRes);
  DDX_Text(pDX, IDC_EDIT_FIXED_PHASE, m_fFixedPhase);
  MinMaxFloat(IDC_EDIT_FIXED_PHASE, m_fFixedPhase, -20, 200, "Fixed phase shift");
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
  ON_EN_KILLFOCUS(IDC_EDIT_FIXED_PHASE, &CCtffindParamDlg::OnKillfocusEditFixedPhase)
END_MESSAGE_MAP()


// CCtffindParamDlg message handlers

// Initialize dialog
BOOL CCtffindParamDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  UpdateSettings();
  SetDefID(45678);    // Disable OK from being default button
  return TRUE;
}

// Load the varaibales for initial or changed settings
void CCtffindParamDlg::UpdateSettings()
{
  mProcessImage = mWinApp->mProcessImage;
  m_bSlowSearch = mProcessImage->GetSlowerCtfFit();
  m_bExtraStats = mProcessImage->GetExtraCtfStats();
  m_fCustomMaxRes = mProcessImage->GetUserMaxCtfFitRes();
  m_bDrawRings = mProcessImage->GetDrawExtraCtfRings() != 0;
  m_fPhaseMinRes = mProcessImage->GetMinCtfFitResIfPhase();
  m_bFindPhase = mProcessImage->GetCtfFindPhaseOnClick();
  m_bFixAstigForPhase = mProcessImage->GetCtfFixAstigForPhase();
  m_iMinPhase = mProcessImage->GetCtfMinPhase();
  m_iMaxPhase = mProcessImage->GetCtfMaxPhase();
  m_fFixedPhase = (float)(mProcessImage->GetPlatePhase() / DTOR);
  m_strDfltMaxRes.Format("(0 for default = %.1f A; applies to script fitting)",
    mProcessImage->GetDefaultMaxCtfFitRes());
  UpdateData(false);
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
  UpdateData(true);
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

// Draw extra rings
void CCtffindParamDlg::OnCheckDrawRings()
{
  UpdateData(true);
  mProcessImage->SetDrawExtraCtfRings(m_bDrawRings ? 1 : 0);
  mWinApp->RestoreViewFocus();
}

// Slower search
void CCtffindParamDlg::OnCheckSlowSearch()
{
  UpdateData(true);
  mProcessImage->SetSlowerCtfFit(m_bSlowSearch);
  RefitToCurrentFFT();
}

// Max res for fitting, limited to 3 A
void CCtffindParamDlg::OnKillfocusEditMaxFitRes()
{
  UpdateData(true);
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
  UpdateData(true);
  mProcessImage->SetMinCtfFitResIfPhase(m_fPhaseMinRes);
  RefitToCurrentFFT();
}

// Find phase
void CCtffindParamDlg::OnCheckFindPhase()
{
  UpdateData(true);
  mProcessImage->SetCtfFindPhaseOnClick(m_bFindPhase);
  RefitToCurrentFFT();
}

// Fix astigmatism when finding phase
void CCtffindParamDlg::OnCheckFixAstigForPhase()
{
  UpdateData(true);
  mProcessImage->SetCtfFixAstigForPhase(m_bFixAstigForPhase);
  RefitToCurrentFFT();
}

// Get extra stats for res fit to
void CCtffindParamDlg::OnCheckExtraStats()
{
  UpdateData(true);
  mProcessImage->SetExtraCtfStats(m_bExtraStats);
  RefitToCurrentFFT();
}

// Fixed phase
void CCtffindParamDlg::OnKillfocusEditFixedPhase()
{
  UpdateData(true);
  mProcessImage->SetPlatePhase((float)DTOR * m_fFixedPhase);
  RefitToCurrentFFT();
}

// Min and max phase for fitting
void CCtffindParamDlg::OnEnKillfocusEditMinPhase()
{
  UpdateData(true);
  if (m_iMinPhase > m_iMaxPhase) {
    m_iMinPhase = m_iMaxPhase - 1;
    UpdateData(false);
  }
  mProcessImage->SetCtfMinPhase(m_iMinPhase);
  RefitToCurrentFFT();
}

void CCtffindParamDlg::OnEnKillfocusEditMaxPhase()
{
  UpdateData(true);
  if (m_iMinPhase > m_iMaxPhase) {
    m_iMinPhase = m_iMaxPhase - 1;
    UpdateData(false);
  }
  mProcessImage->SetCtfMaxPhase(m_iMaxPhase);
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
