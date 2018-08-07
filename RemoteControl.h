#pragma once
#include "afxwin.h"
#include "afxcmn.h"

class EMscope;

// CRemoteControl dialog

class CRemoteControl : public CToolDlg
{

public:
	CRemoteControl(CWnd* pParent = NULL);   // standard constructor
	virtual ~CRemoteControl();

// Dialog Data
	enum { IDD = IDD_REMOTE_CONTROL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
  void Update(int inMag, int inSpot, double inIntensity, int inProbe, int inGun, 
    int inSTEM);
  void UpdateSettings();
  void UpdateEnables(void);
  void ChangeIntensityByIncrement(int delta);
  GetMember(float, BeamIncrement);
  GetMember(float, IntensityIncrement);
  void SetBeamIncrement(float inVal);
  void SetIntensityIncrement(float inVal);
  CButton m_butValves;
  afx_msg void OnButValves();
  CButton m_butNanoMicro;
  afx_msg void OnButNanoMicro();
  CButton m_checkMagIntensity;
  BOOL m_bMagIntensity;
  afx_msg void OnCheckMagIntensity();
  CSpinButtonCtrl m_sbcMag;
  afx_msg void OnDeltaposSpinMag(NMHDR *pNMHDR, LRESULT *pResult);
  CSpinButtonCtrl m_sbcSpot;
  afx_msg void OnDeltaposSpinSpot(NMHDR *pNMHDR, LRESULT *pResult);
  CSpinButtonCtrl m_sbcBeamShift;
  afx_msg void OnDeltaposSpinBeamShift(NMHDR *pNMHDR, LRESULT *pResult);
  CSpinButtonCtrl m_sbcIntensity;
  afx_msg void OnDeltaposSpinIntensity(NMHDR *pNMHDR, LRESULT *pResult);
  CButton m_butDelBeamMinus;
  afx_msg void OnButDelBeamMinus();
  CButton m_butDelBeamPlus;
  afx_msg void OnButDelBeamPlus();
  CButton m_butDelC2Minus;
  afx_msg void OnButDelC2Minus();
  CButton m_butDelC2Plus;
  afx_msg void OnButDelC2Plus();
  CString m_strBeamDelta;
  CString m_strC2Delta;

private:
  int mLastMagInd;
  int mLastGunOn;
  int mLastSpot;
  int mLastProbeMode;
  int mLastSTEMmode;
  double mLastIntensity;
  int mLastCamera;
  CEMscope *mScope;
  float mBeamIncrement;
  float mIntensityIncrement;
  bool mSpotClicked;
  bool mMagClicked;
  int mNewSpotIndex;
  int mNewMagIndex;
  int mStartMagIndex;
  UINT_PTR mTimerID;
  int mMaxClickInterval;
  bool mCtrlPressed;
  bool mDidExtendedTimeout;
public:
  CSpinButtonCtrl m_sbcBeamLeftRight;
  afx_msg void OnDeltaposSpinBeamLeftRight(NMHDR *pNMHDR, LRESULT *pResult);
  CString m_strC2Name;
  bool mInitialized;
  void SetMagOrSpot(void);
  void GetPendingMagOrSpot(int &pendingMag, int &pendingSpot);
  void CtrlChanged(bool pressed);
};
