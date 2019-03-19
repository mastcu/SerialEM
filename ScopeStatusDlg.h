#if !defined(AFX_SCOPESTATUSDLG_H__A432B655_54DA_4655_8DCC_B7ADF56ADA0B__INCLUDED_)
#define AFX_SCOPESTATUSDLG_H__A432B655_54DA_4655_8DCC_B7ADF56ADA0B__INCLUDED_

#include "UTILITIES\SMOOTHER.H"	// Added by ClassView
#include "ScreenMeter.h"
#include "DoseMeter.h"
#include "afxwin.h"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ScopeStatusDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CScopeStatusDlg dialog

class CScopeStatusDlg : public CToolDlg
{
// Construction
public:
  BOOL WatchingDose() {return mDoseMeter != NULL || mWatchDose;};
	void AddToDose(double dose);
	void OutputDose();
	WINDOWPLACEMENT * GetDosePlacement();
	void DoseClosing();
	WINDOWPLACEMENT *GetMeterPlacement();
	GetSetMember(float, CurrentLogBase)
	void SetCurrentSmootherThreshold1(float inVal) {mSmootherThreshold1 = inVal;};
  float GetCurrentSmootherThreshold1() {return mSmootherThreshold1;};
	void SetCurrentSmootherThreshold2(float inVal) {mSmootherThreshold2 = inVal;};
  float GetCurrentSmootherThreshold2() {return mSmootherThreshold2;};
	GetSetMember(int, FloatingMeterSmoothed)
	void Update(double inCurrent, int inMagInd, double inDefocus, double inISX,
		double inISY, double inStageX, double inStageY, double inStageZ, BOOL screenUp, BOOL smallScreen, BOOL blanked, 
    BOOL EFTEM, int STEM, int inSpot, double rawIntensity, double inIntensity, double inObjective, 
    int inVacStatus, double cameraLength, int inProbeMode, int gunState, int alpha);
	CScopeStatusDlg(CWnd* pParent = NULL);   // standard constructor
	void MeterClosing();
  GetMember(double, FullCumDose)
  SetMember(BOOL, WatchDose)
  SetMember(BOOL, ShowIntensityCal);

// Dialog Data
	//{{AFX_DATA(CScopeStatusDlg)
	enum { IDD = IDD_SCOPESTATUS };
	CStatic	m_statXLM;
	CStatic	m_statNano;
	CButton	m_butDose;
	CStatic	m_statUmMm;
	CStatic	m_statVacuum;
	CStatic	m_statSpotSize;
	CStatic	m_statObjective;
	CButton	m_butFloat;
	CStatic	m_statCurrent;
	CStatic	m_statMag;
	CStatic	m_statImageShift;
	CStatic	m_statDefocus;
	CString	m_strDefocus;
	CString	m_strImageShift;
	CString	m_strMag;
	CString	m_strCurrent;
	CString	m_strObjective;
	CString	m_strSpotSize;
	CString	m_strUmMm;
	CString	m_strVacuum;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CScopeStatusDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CScopeStatusDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnButfloat();
  afx_msg void OnPaint();
	afx_msg void OnButdose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	BOOL mInitialized;
	Smoother mCurrentSmoother;
	// Storage for last values that were sent in
	double mCurrent;		// screen current in corrected na
  int mMagInd;        // Magnification index last displayed
	int mMag;				    // Magnification last displayed
  int mPendingMag;    // Pending mag on last display
  int mSTEM;          // STEM mode last displayed
	double mStageX, mStageY;	// Stage positions in microns
  double mStageZ;
	double mISX, mISY;		// Image shift X and Y in microns
	double mDefocus;		// Defocus in microns
  double mDoseRate;   // Dose rate
  bool mShowedDose;     // Flag that dose was shown last time
	CFont mBigFont;			// Font for bigger values
	CFont mMedFont;			// Medium font for others
  CFont mProbeFont;   // Font for the Spot/nPr indicator
	CScreenMeter *mScreenMeter;
	double mMeterCurrent;	// Current last displayed on floating meter
	int mFloatingMeterSmoothed;
	float mCurrentLogBase;
	float mSmootherThreshold1;
	float mSmootherThreshold2;
	int mSpot;			      // Last spot size
  int mPendingSpot;     // Last pending spot size
	double mIntensity;		// Last C2 intensity
	double mObjective;		// Last Objective lens val
  int mVacStatus;       // Last Vacuum status index
  int mIntCalStatus;       // Last intensity status index
  double mCamLength;    // Last camera length
  int mBeamAlpha;       // Last alpha from JEOL
  BOOL mLastCooker;     // Flag that cooker was open last time
  BOOL mLastAutocen;    // Flag that beam autocenter was open last time
  double mRawIntensity; // Last raw intensity
  int mCameraIndex;     // Last current camera index
  WINDOWPLACEMENT mMeterPlace;
  CDoseMeter *mDoseMeter;
  WINDOWPLACEMENT mDosePlace;
  double mLastDoseTime;
  BOOL mSmallScreen;    // Last small screen state
  double mFullCumDose;  // Cumulative dose from program start
  BOOL mWatchDose;      // Flag to watch for dose even if meter not open
  BOOL mShowIntensityCal;  // Flag to show colored intensity calibration
  BOOL mTEMnanoProbe;   // Flag that nanoprobe is on in TEM
public:
  CButton m_butResetDef;
  afx_msg void OnResetDefocus();
  CStatic m_statSpotLabel;
  CString m_strIntensity;
  CStatic m_statIntensity;
  CStatic m_statC23IA;
  CStatic m_statPctUm;
  CStatic m_statStageX;
  CStatic m_statStageY;
  CStatic m_statStageZ;
  CStatic m_statZlabel;
  CString m_strDoseRate;
  CStatic m_statDoseRate;
  CStatic m_statUbpix;
  int DoseHeightAdjustment(void);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SCOPESTATUSDLG_H__A432B655_54DA_4655_8DCC_B7ADF56ADA0B__INCLUDED_)
