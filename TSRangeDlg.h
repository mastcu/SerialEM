#pragma once
#include "BaseDlg.h"
#include "afxwin.h"


// CTSRangeDlg dialog

class CTSRangeDlg : public CBaseDlg
{
public:
	CTSRangeDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTSRangeDlg();

// Dialog Data
	enum { IDD = IDD_TSRANGE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
  int m_iLowDose;
  afx_msg void OnRadioRegmode();
  float m_fLowAngle;
  float m_fHighAngle;
  float m_fIncrement;
  int m_iImageType;
  BOOL mDoAcquire;
  RangeFinderParams *mParams;
  void LoadParameters(int set);
  void UnloadParameters(int set);
  BOOL m_bEucentricity;
  BOOL m_bWalkup;
  BOOL m_bAutofocus;
  CButton m_butPreview;
  void ManageImageTypes(void);
  int m_iDirection;
};
