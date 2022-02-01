#pragma once
#include "BaseDlg.h"
#include "afxwin.h"
#include "afxcmn.h"


// CAutocenSetupDlg dialog

class CAutocenSetupDlg : public CBaseDlg
{
public:
	CAutocenSetupDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CAutocenSetupDlg();

// Dialog Data
	enum { IDD = IDD_AUTOCENSETUP };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);

	DECLARE_MESSAGE_MAP()

private:
  CEMscope *mScope;
  CMultiTSTasks *mMultiTasks;
  CameraParameters *mCamParams;
  MagTable *mMagTab;
  int mBinIndex;
  bool mSynthesized;
  int mBestMag;
  int mBestSpot;
  bool mEnableAll;
  double mStartingBSX, mStartingBSY;
  BOOL mLowDoseMode;
  bool mLastTrialMismatch;
  bool mOpening;

public:

  int mSavedMagInd;
  double mSavedIntensity;
  int mSavedSpot;
  int mSavedProbe;
  int mCamera;
  int mCurMagInd;
  int mCurSpot;
  int mCurProbe;
  AutocenParams *mParam;
  double mCurIntensity;
  CStatic m_statCamLabel1;
  CStatic m_statCamLabel2;
  CStatic m_statCamName;
  CString m_strCamName;
  BOOL m_bSetState;
  CString m_strSpot;
  CString m_strIntensity;
  CString m_strBinning;
  CSpinButtonCtrl m_sbcSpot;
  CSpinButtonCtrl m_sbcBinning;
  float m_fExposure;
  CButton m_butAcquire;
  int m_iUseCentroid;
  afx_msg void OnSetbeamstate();
  afx_msg void OnDeltaposSpinspot(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnDeltaposSpinbinning(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnRadiouseedge();
  afx_msg void OnTestacquire();
  CString m_strMag;
  CSpinButtonCtrl m_sbcMag;
  afx_msg void OnDeltaposSpinmag(NMHDR *pNMHDR, LRESULT *pResult);
  void LiveUpdate(int magInd, int spotSize, int probe, double intensity);
  CButton m_butDelete;
  afx_msg void OnDeleteSettings();
  void ParamChanged(void);
  void UpdateParamSettings(void);
  void StartTrackingState(void);
  void UpdateMagSpot(void);
  void MagOrSpotChanged(int magInd, int spot, int probe);
  void FetchParams(void);
  void RestoreScopeState(void);
  BOOL UpdateIfExposureChanged(void);
  CString m_strSource;
  CButton m_butMore;
  CButton m_butLess;
  afx_msg void OnButAcMore();
  afx_msg void OnButAcLess();
  afx_msg void OnKillfocusEditExposure();
  CStatic m_statNanoprobe;
  CButton m_butSetState;
  CEdit m_editExposure;
  CButton m_butUseEdge;
  CButton m_butUseCentroid;
  void UpdateEnables();
  void UpdateSettings();
  CButton m_butShiftBeam;
  CStatic m_statMicrons;
  CEdit m_editBeamShift;
  BOOL m_bShiftBeam;
  float m_fBeamShift;
  afx_msg void OnShiftBeam();
  afx_msg void OnEnKillfocusEditBeamShift();
  void ManageShiftBeam();
  CString m_strAddedShift;
  BOOL m_bRecordingAddedShift;
  afx_msg void OnRecordAddedShift();
  CButton m_butResetAddedShift;
  afx_msg void OnResetAddedShift();
  double GetParamIntensity() { return mParam ? mParam->intensity : 0.; };
  CButton m_butRecordShift;
  void MoreOrLessIntensity(double factor);
  CStatic m_statLdTrackScope;
  void ManageLDtrackText(bool tracking);
  CButton m_butUseTrialSmaller;
  BOOL m_bUseTrialSmaller;
  CStatic m_statSmallerTrial;
  CSpinButtonCtrl m_sbcSmallerTrial;
  afx_msg void OnDeltaposSpinSmallerTrial(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnUseTrialSmaller();
  void ManageSettingsLines();
  CString m_strSmallerTrial;
};
