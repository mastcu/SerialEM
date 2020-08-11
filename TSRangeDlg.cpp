// TSRangeDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\TSRangeDlg.h"
#include "MultiTSTasks.h"


// CTSRangeDlg dialog

CTSRangeDlg::CTSRangeDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CTSRangeDlg::IDD, pParent)
  , m_iLowDose(0)
  , m_fLowAngle(0)
  , m_fHighAngle(0)
  , m_fIncrement(0)
  , m_iImageType(0)
  , m_bEucentricity(FALSE)
  , m_bWalkup(FALSE)
  , m_bAutofocus(FALSE)
  , m_iDirection(0)
{
}

CTSRangeDlg::~CTSRangeDlg()
{
}

void CTSRangeDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_RADIO_TSRPREVIEW, m_butPreview);
  DDX_Text(pDX, IDC_EDIT_TSR_LOWANGLE, m_fLowAngle);
  DDX_Text(pDX, IDC_EDIT_TSR_HIGHANGLE, m_fHighAngle);
  DDX_Text(pDX, IDC_EDIT_TSR_INCANGLE, m_fIncrement);
  MinMaxFloat(IDC_EDIT_TSR_INCANGLE, m_fIncrement, 0.0f, 10.f, "Angle increment");
  MinMaxFloat(IDC_EDIT_TSR_LOWANGLE, m_fLowAngle, 0.0f, 90.f, "Low angle");
  MinMaxFloat(IDC_EDIT_TSR_HIGHANGLE, m_fHighAngle, 0.0f, 90.f, "High angle");
  DDX_Check(pDX, IDC_TSR_EUCENTRICITY, m_bEucentricity);
  DDX_Check(pDX, IDC_TSR_WALKUP, m_bWalkup);
  DDX_Check(pDX, IDC_TSR_AUTOFOCUS, m_bAutofocus);
  DDX_Radio(pDX, IDC_RADIOREGMODE, m_iLowDose);
  DDX_Radio(pDX, IDC_RADIO_TSRTRIAL, m_iImageType);
  DDX_Radio(pDX, IDC_RADIO_TSR_BOTHDIR, m_iDirection);
}


BEGIN_MESSAGE_MAP(CTSRangeDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_RADIOREGMODE, OnRadioRegmode)
  ON_BN_CLICKED(IDC_RADIOTSRLOWDOSE, OnRadioRegmode)
END_MESSAGE_MAP()


// CTSRangeDlg message handlers

BOOL CTSRangeDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  mParams = mWinApp->GetTSRangeParams();
  m_iLowDose = mWinApp->LowDoseMode() ? 1 : 0;
  LoadParameters(m_iLowDose);
  ManageImageTypes();

  UpdateData(false);
  return TRUE;
}


void CTSRangeDlg::ManageImageTypes(void)
{
  CString *names = mWinApp->GetModeNames();
  int *settypes = mWinApp->mMultiTSTasks->GetRangeConsets();
  SetDlgItemText(IDC_RADIO_TSRTRIAL, names[settypes[3 * m_iLowDose]]);
  SetDlgItemText(IDC_RADIO_TSRRECORD, names[settypes[1 + 3 * m_iLowDose]]);
  SetDlgItemText(IDC_RADIO_TSRPREVIEW, names[settypes[5]]);
  m_butPreview.ShowWindow(m_iLowDose > 0 ? SW_SHOW : SW_HIDE);
}

void CTSRangeDlg::OnOK() 
{
  UpdateData(true);
  UnloadParameters(m_iLowDose);
	CBaseDlg::OnOK();
}

void CTSRangeDlg::OnCancel() 
{
  CBaseDlg::OnCancel();
}


void CTSRangeDlg::OnRadioRegmode()
{
  int oldSet = m_iLowDose;
  UpdateData(true);
  UnloadParameters(oldSet);
  LoadParameters(m_iLowDose);
  ManageImageTypes();
  UpdateData(false);
}

// Load parameters from parameter set into dialog variables
void CTSRangeDlg::LoadParameters(int set)
{
  m_bEucentricity = mParams[set].eucentricity;
  m_bWalkup = mParams[set].walkup;
  m_bAutofocus = mParams[set].autofocus;
  m_iImageType = mParams[set].imageType;
  m_fLowAngle = mParams[set].startAngle;
  m_fHighAngle = mParams[set].endAngle;
  m_fIncrement = mParams[set].angleInc;
  m_iDirection = mParams[set].direction;
}

// Unload parameters from dialog variables to parameter set
void CTSRangeDlg::UnloadParameters(int set)
{
  mParams[set].eucentricity = m_bEucentricity;
  mParams[set].walkup = m_bWalkup;
  mParams[set].autofocus = m_bAutofocus;
  mParams[set].imageType = m_iImageType;
  mParams[set].startAngle = m_fLowAngle;
  mParams[set].endAngle = m_fHighAngle;
  mParams[set].angleInc = m_fIncrement;
  mParams[set].direction = m_iDirection;
  mParams[set].endAngle = B3DMAX(mParams[set].startAngle, mParams[set].endAngle);
}
