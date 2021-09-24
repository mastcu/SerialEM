#pragma once
#include "afxwin.h"
#include "ParticleTasks.h"

class CEMscope;

// ZbyGSetupDlg dialog

class CZbyGSetupDlg : public CBaseDlg
{
public:
	CZbyGSetupDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CZbyGSetupDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_Z_BY_G_SETUP };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);

	DECLARE_MESSAGE_MAP()

private:
  CEMscope *mScope;
  CParticleTasks *mParticleTasks;
  CArray<ZbyGParams> *mCalArray;
  ZbyGParams mCurParams;     // Current state, in a param ready to use
  ZbyGParams *mCalParams;    // Pointer to entry in array if there is a current cal
  int mIndexOfCal;           // Index in array of that cal
  int mHasAlpha;             // flag for whether to display alpha
  bool mRecalEnabled;        // Flag if cal can be used for recal

public:
  CString m_strCurMag;
  CString m_strCalMag;
  CString m_strCurSpot;
  CString m_strCalSpot;
  CString m_strProbeLabel;
  CString m_strCurProbe;
  CString m_strCalProbe;
  CString m_strCurCam;
  CString m_strCurOffset;
  CString m_strCalOffset;
  afx_msg void OnButUpdateState();
  BOOL m_bUseOffset;
  afx_msg void OnCheckUseOffset();
  float m_fFocusOffset;
  CEdit m_editFocusOffset;
  float m_fBeamTilt;
  afx_msg void OnButUseCurToCal();
  afx_msg void OnButUseStoredToRecal();
  void StartCalibration(int which);
  CButton m_butUseToCal;
  CButton m_butUseToRecal;
  BOOL m_bUseViewInLD;
  int m_iMaxChange;
  float m_fIterThresh;
   CString m_strCurC2;
  CString m_strCalC2;
  void GetCurrentState();
  void UpdateCalStateForMag();
  void UpdateSettings();
  void UpdateEnables();
  bool FocusCalExistsForParams(ZbyGParams *params);
  CButton m_butUpdateState;
  afx_msg void OnCheckUseViewInLd();
  afx_msg void OnKillfocusEditBox();
  void UnloadControlParams();
  CString m_strMradLabel;
  CEdit m_editBeamTilt;
  BOOL m_bCalWithBT;
  afx_msg void OnCheckCalWithBt();
  CString m_strCurBeamTilt;
  CString m_strCalBeamTilt;
  int m_iViewSubarea;
  afx_msg void OnRadioViewSubset();
};
