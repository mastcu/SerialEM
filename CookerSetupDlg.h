#pragma once
#include "BaseDlg.h"
#include "afxcmn.h"
#include "ControlSet.h"
#include "afxwin.h"

class CEMscope;

// CCookerSetupDlg dialog

class CCookerSetupDlg : public CBaseDlg
{

public:
	CCookerSetupDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CCookerSetupDlg();

// Dialog Data
	enum { IDD = IDD_COOKSETUP };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()

private:
  CEMscope *mScope;
  int mSavedSpot;
  int mSavedMagInd;
  double mSavedIntensity;
  CookParams *mCookP;
  int mCurSpot;
  int mCurMagInd;
  double mCurIntensity;
  CFont mBigFont;

public:
  CString m_strMag;
  CString m_strSpot;
  CString m_strIntensity;
  CString m_strDoseRate;
  CString m_strTotalTime;
  BOOL m_bTrackState;
  BOOL m_bAlignCooked;
  BOOL m_bGoCook;
  CSpinButtonCtrl m_sbcDose;
  int m_iDose;
  int m_iTimeInstead;
  afx_msg void OnSetTrackState();
  afx_msg void OnAligncooked();
  afx_msg void OnDeltaposSpincookdose(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnKillfocusEditcookdose();
  void LiveUpdate(int magInd, int spotSize, double intensity);
  void UpdateDoseTime(void);
  void SetScopeState(void);
  void RestoreScopeState(void);

  void UpdateMagBeam(void);
  BOOL m_bTiltForCook;
  CEdit m_editCookTilt;
  float m_fTiltAngle;
  afx_msg void OnTiltforcook();
  afx_msg void OnKillfocusEditcooktilt();
  CEdit m_editCookTime;
  float m_fTime;
  afx_msg void OnRadiodose();
  afx_msg void OnKillfocusEditcooktime();
  CSpinButtonCtrl m_sbcCookTime;
  afx_msg void OnDeltaposSpincooktime(NMHDR *pNMHDR, LRESULT *pResult);
  CString m_strDoseTime;
  CEdit m_editCookDose;
  CButton m_butGoCook;
  afx_msg void OnCookgo();
  CStatic m_statDoseTime;
  CStatic m_statTotalTime;
};
