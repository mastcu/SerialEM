#pragma once
#include "afxwin.h"
#include "afxcmn.h"
#include "MyButton.h"

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
  afx_msg void OnPaint();

	DECLARE_MESSAGE_MAP()
public:
  void Update(int inMag, int inCamLen, int inSpot, double inIntensity, int inProbe, int inGun, 
    int inSTEM, int inAlpha, int inScreenPos);
  void UpdateSettings();
  void UpdateEnables(void);
  void ChangeIntensityByIncrement(int delta);
  GetMember(float, BeamIncrement);
  GetMember(float, IntensityIncrement);
  void SetBeamIncrement(float inVal);
  void SetStageIncrementIndex(int inVal);
  void SetIntensityIncrement(float inVal);
  void SetBeamOrStage(int inVal);
  GetMember(int, FocusIncrementIndex);
  GetMember(int, StageIncIndex);
  void SetFocusIncrementIndex(int inVal);
  GetMember(int, DoingTask);
  void SetIncrementFromIndex(float &incrVal, int &incrInd, int newInd, 
  int maxIndex, int maxDecimals, CString &str);
  void SetBeamOrStageIncrement(float beamIncFac, int stageIndAdd);
  CMyButton m_butValves;
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
  int mLastMagInd;              // Values for state saved from last update
  int mLastCamLenInd;
  int mLastGunOn;
  int mLastSpot;
  int mLastProbeMode;
  int mLastSTEMmode;
  double mLastIntensity;
  int mLastCamera;
  int mLastAlpha;
  int mLastScreenPos;
  CEMscope *mScope;             // Convenience pointers
  CShiftManager *mShiftManager;
  float mBeamIncrement;         // Increments for beam and C2  
  float mIntensityIncrement;
  float mStageIncrement;        // Increments for stage and focus 
  int mStageIncIndex;           // and indexes used to determine it
  float mFocusIncrement;
  int mFocusIncrementIndex;
  bool mSpotClicked;            // Flag that spot or mag was clicked
  bool mMagClicked;
  int mNewSpotIndex;            // Current new spot or mag index
  int mNewMagIndex;
  int mNewCamLenIndex;
  int mStartMagIndex;           // Mag at start of any clicking
  UINT_PTR mTimerID;
  int mMaxClickInterval;        // Maximum interval between clicks befor changing
  bool mCtrlPressed;            // Flag that Ctrl was pressed
  bool mDidExtendedTimeout;     // Flag for long timeout with Ctrl pressed started
  int mDoingTask;               // 1 if doing screen, 2 if doing stage
public:
  CSpinButtonCtrl m_sbcBeamLeftRight;
  afx_msg void OnDeltaposSpinBeamLeftRight(NMHDR *pNMHDR, LRESULT *pResult);
  CString m_strC2Name;
  void SetMagOrSpot(void);
  void GetPendingMagOrSpot(int &pendingMag, int &pendingSpot, int &pendingCamLen);
  void CtrlChanged(bool pressed);
  CStatic m_statAlpha;
  CSpinButtonCtrl m_sbcAlpha;
  afx_msg void OnDeltaposSpinAlpha(NMHDR *pNMHDR, LRESULT *pResult);
  CString m_strFocusStep;
  CSpinButtonCtrl m_sbcFocus;
  CButton m_butDelFocusMinus;
  CButton m_butDelFocusPlus;
  afx_msg void OnDeltaposSpinFocus(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnDelFocusMinus();
  afx_msg void OnDelFocusPlus();
  CButton m_butScreenUpDown;
  CButton m_butBeamControl;
  CButton m_butStageControl;
  afx_msg void OnButScreenUpdown();
  void TaskDone(int param);
  void TaskCleanup(int error);
  afx_msg void OnBeamControl();
  int m_iStageNotBeam;
  void MoveStageByMicronsOnCamera(double delCamX, double delCamY);
  void ChangeDiffShift(int deltaX, int deltaY);
};
