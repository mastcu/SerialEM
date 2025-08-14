#pragma once
#include "afxwin.h"

struct CamPluginFuncs;
struct DectrisPlugFuncs;

// CDectrisSettingsDlg dialog

class CDectrisSettingsDlg : public CBaseDlg
{

public:
	CDectrisSettingsDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDectrisSettingsDlg();
  void LoadComboBox(CComboBox &combo, float *energies);
  void LookupAndSetEnergyIndex(float *energies, float value,
    int &index, const char *descrip);

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DECTRIS_SETTINGS };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();

	DECLARE_MESSAGE_MAP()


public:
  int mDectrisTypeInd;
  DectrisPlugFuncs *mDectrisPlugFuncs;
  DectrisAdvancedSettings mSettings;
  CComboBox m_comboIncidentKv;
  CComboBox m_comboThresholdKv;
  BOOL m_bAutosum;
  BOOL m_bCountrateCorr;
  BOOL m_bRetrigger;
  BOOL m_bDCUFlatCorr;
  BOOL m_bBadPixInterp;
  BOOL m_bModuleInterp;
  int m_iIncidentKv;
  int m_iThresholdKv;
};
