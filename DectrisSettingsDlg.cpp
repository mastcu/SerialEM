// DectrisSettingsDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "DectrisSettingsDlg.h"
#include "PluginManager.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

// Each energy list MUST end in a 0 for auto
static float sIncidentEnergies[3][10] = {{30, 40, 60, 80, 100, 120, 160, 200, 300, 0},
{60, 80, 100, 120, 160, 200, 300, 0}, {0}};
static int sIncidentMap[MAX_DECTRIS_TYPES] = {0, 0, 0, 2, 0, 1};

static float sThresholdEnergies[3][9] = {{2.7f, 3.0f, 3.5f, 4.0f, 4.5f, 5.0f, 10.f,
25.f, 0}, {10.f, 17.5f, 43.f, 0}, {0}};
static int sThresholdMap[MAX_DECTRIS_TYPES] = {2, 2, 2, 0, 2, 1};

// CDectrisSettingsDlg dialog

CDectrisSettingsDlg::CDectrisSettingsDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_DECTRIS_SETTINGS, pParent)
  , m_bBadPixInterp(FALSE)
  , m_bModuleInterp(FALSE)
  , m_iIncidentKv(0)
  , m_iThresholdKv(0)
{

}

CDectrisSettingsDlg::~CDectrisSettingsDlg()
{
}

void CDectrisSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_COMBO_INCIDENT_KV, m_comboIncidentKv);
  DDX_Control(pDX, IDC_COMBO_THRESHOLD_KV, m_comboThresholdKv);
  DDX_Check(pDX, IDC_CHECK_AUTOSUM, m_bAutosum);
  DDX_Check(pDX, IDC_CHECK_COUNTRATE_CORR, m_bCountrateCorr);
  DDX_Check(pDX, IDC_CHECK_RETRIGGER, m_bRetrigger);
  DDX_Check(pDX, IDC_CHECK_DCU_FLAT_CORR, m_bDCUFlatCorr);
  DDX_Check(pDX, IDC_CHECK_BAD_PIX_INTERP, m_bBadPixInterp);
  DDX_Check(pDX, IDC_CHECK_MODULE_INTERP, m_bModuleInterp);
  DDX_CBIndex(pDX, IDC_COMBO_INCIDENT_KV, m_iIncidentKv);
  DDX_CBIndex(pDX, IDC_COMBO_THRESHOLD_KV, m_iThresholdKv);
}

BOOL CDectrisSettingsDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  B3DCLAMP(mDectrisTypeInd, 0, MAX_DECTRIS_TYPES - 1);
  ShowDlgItem(IDC_CHECK_MODULE_INTERP, mDectrisTypeInd == 2 || mDectrisTypeInd == 3);
  LoadComboBox(m_comboIncidentKv, &sIncidentEnergies[sIncidentMap[mDectrisTypeInd]][0]);
  LoadComboBox(m_comboThresholdKv,
    &sThresholdEnergies[sThresholdMap[mDectrisTypeInd]][0]);

  // Get the settings
  if (mDectrisPlugFuncs->GetDectrisAdvancedSettings(&mSettings,
    sizeof(DectrisAdvancedSettings))) {
    AfxMessageBox("The request to the plugin for the current advanced settings returned"
      " with an error", MB_EXCLAME);
    OnCancel();
    return false;
  }
  LookupAndSetEnergyIndex(&sIncidentEnergies[sIncidentMap[mDectrisTypeInd]][0],
    mSettings.incidentEnergy, m_iIncidentKv, "Incident energy");
  LookupAndSetEnergyIndex(&sThresholdEnergies[sThresholdMap[mDectrisTypeInd]][0],
    mSettings.thresholdEnergy, m_iThresholdKv, "Threshold energy");
  m_bAutosum = mSettings.autosummation;
  m_bRetrigger = mSettings.retrigger;
  m_bCountrateCorr = mSettings.countrateCorrection;
  m_bDCUFlatCorr = mSettings.dcuInternalCorrection;
  m_bBadPixInterp = mSettings.badPixelInterpolation;
  m_bModuleInterp = mSettings.moduleGapInterpolation;
  UpdateData(false);

  return TRUE;
}

void CDectrisSettingsDlg::OnOK()
{
  UpdateData(true);
  mSettings.autosummation = m_bAutosum;
  mSettings.retrigger = m_bRetrigger;
  mSettings.countrateCorrection = m_bCountrateCorr;
  mSettings.dcuInternalCorrection = m_bDCUFlatCorr;
  mSettings.badPixelInterpolation = m_bBadPixInterp;
  mSettings.moduleGapInterpolation = m_bModuleInterp;
  mSettings.incidentEnergy =
    sIncidentEnergies[sIncidentMap[mDectrisTypeInd]][m_iIncidentKv];
  mSettings.thresholdEnergy =
    sThresholdEnergies[sThresholdMap[mDectrisTypeInd]][m_iThresholdKv];

  // Apply the settings
  if (mDectrisPlugFuncs->SetDectrisAdvancedSettings(&mSettings,
    sizeof(DectrisAdvancedSettings))) {
    AfxMessageBox("The call to set the advanced settings returned with an error",
      MB_EXCLAME);
  }
  CBaseDlg::OnOK();
}


BEGIN_MESSAGE_MAP(CDectrisSettingsDlg, CBaseDlg)
END_MESSAGE_MAP()


// CDectrisSettingsDlg message handlers

// Load the values for the camera type into a combo box
void CDectrisSettingsDlg::LoadComboBox(CComboBox &combo, float *energies)
{
  CString str;
  int ind, num = 0;
  for (ind = 0; ind < 20; ind++) {
    if (energies[ind] < 0)
      break;
    if (!energies[ind])
      str = "Auto";
    else
      str.Format("%g", energies[ind]);
    combo.AddString((LPCTSTR)str);
    num++;
    if (!energies[ind])
      break;
  }
  SetDropDownHeight(&combo, num);
}

// Look up the value from the plugin in the table for the camera type
void CDectrisSettingsDlg::LookupAndSetEnergyIndex(float *energies,
  float value, int &index, const char *descrip)
{
  float minDiff = 1.e10, diff;
  int ind, sel;
  for (ind = 0; ind < 20; ind++) {
    if (energies[ind] < 0)
      break;
    diff = B3DABS(energies[ind] - value);
    if (diff < minDiff) {
      minDiff = diff;
      sel = ind;
    }
    if (!energies[ind])
      break;
  }
  if (minDiff > 1.)
    PrintfToLog("WARNING: %s from DECTRIS plugin does not match any of the expected "
      "values exactly", descrip);
  index = sel;
}

