#include "afxwin.h"
#include "afxcmn.h"
#if !defined(AFX_LOWDOSEDLG_H__7DD4D5B2_CBF6_4DD3_902E_EAF16F469071__INCLUDED_)
#define AFX_LOWDOSEDLG_H__7DD4D5B2_CBF6_4DD3_902E_EAF16F469071__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LowDoseDlg.h : header file
//

#include "MyButton.h"

#define SEARCH_AREA 4

/////////////////////////////////////////////////////////////////////////////
// CLowDoseDlg dialog

class CShiftManager;
class CEMscope;

class CLowDoseDlg : public CToolDlg
{
// Construction
public:
  void SetSearchName(CString inStr) {mSearchName = inStr;};
	void SyncFilterSettings(int inArea, FilterParams *filtParam = NULL);
	void SetContinuousUpdate(BOOL state);
  BOOL GetContinuousUpdate() {return m_bContinuousUpdate;};
	void TransferBaseIS(int mag, double &ISX, double &ISY);
	void TransferISonAxis(int fromMag, double fromX, double fromY, int toMag, double &toX,
    double &toY);
	void ConvertOneAxisToIS(int mag, double axis, double &ISX, double &ISY);
	double ConvertOneIStoAxis(int mag, double ISX, double ISY);
	void FinishSettingBeamShift(BOOL toggled);
	void AreaChanging(int inArea);
	void BlankingUpdate(BOOL blanked);
	void TurnOffDefine();
	void ConvertAxisPosition(BOOL axisToIS);
	void ShiftImageInA(double delAxis);
	void ChangeAllShifts(double delAxis);
	void UpdateSettings();
  void UpdateDefocusOffset();
	void SetLowDoseMode(BOOL inVal, BOOL hideOffState = FALSE);
	void AddDefineAreaPoint();
	void OnOK();
  bool SameAsFocusArea(int inArea);
	void GetAreaSizes(int &recSize, int &defSize, double &angle);
	void UserPointChange(float &ptX, float &ptY, EMimageBuffer *imBuf);
	void ManageAxisPosition();
	void SnapCameraShiftToAxis(EMimageBuffer *imBuf, float &shiftX, float &shiftY, bool viewImage,
    int camIndex, BOOL &rotateAxis, int &axisAngle);
	int UsefulImageInA();
	void FixUserPoint(EMimageBuffer *imBuf, int needDraw);
	BOOL ImageAlignmentChange(float &newX, float &newY, float oldX, float oldY,
		BOOL mouseShifting);
	void SetIgnoreIS() {mIgnoreIS = true;};
	void ManageMagSpot(int inSetArea, BOOL screenDown);
	void Update();
  BOOL GetDefining() {return m_iDefineArea > 0;};
	void ScopeUpdate(int magIndex, int spotSize, double intensity, double ISX, double ISY,
    BOOL screenDown, int inSetArea, int camLenIndex, double focus, float alpha,
    int probeMode);
  bool DoingContinuousUpdate(int inSetArea);
	CLowDoseDlg(CWnd* pParent = NULL);   // standard constructor
  GetSetMember(int, AxisPiezoPlugNum);
  GetSetMember(int, AxisPiezoNum);
  SetMember(float, PiezoFullDelay);
  SetMember(float, FocusPiezoDelayFac);
  SetMember(float, TVPiezoDelayFac);
  GetMember(BOOL, TieFocusTrialPos);


// Dialog Data
	//{{AFX_DATA(CLowDoseDlg)
	enum { IDD = IDD_LOWDOSE };
	CStatic	m_statLDArea;
	CButton	m_butResetBeamShift;
	CButton	m_butSetBeamShift;
	CStatic	m_statBeamShift;
	CButton	m_butNormalizeBeam;
	CStatic	m_statElecDose;
	CStatic	m_statBlanked;
	CButton	m_butUnblank;
	CButton	m_butCopyToRecord;
	CButton	m_butCopyToTrial;
	CButton	m_butCopyToFocus;
	CButton	m_butCopyToView;
	CMyButton	m_butBalanceShifts;
	CButton	m_butContinuousUpdate;
	CButton	m_butTieFocusTrial;
	CButton	m_butLowDoseMode;
	CStatic	m_statPosition;
	CStatic	m_statMicron;
	CButton	m_butDefineNone;
	CButton	m_butDefineFocus;
	CButton	m_butDefineTrial;
	CEdit	m_editPosition;
	CStatic	m_statOverlap;
	CStatic	m_statMagSpot;
	CButton	m_butShowBox;
	CButton	m_butDefineBox;
	int		m_iDefineArea;
	CString	m_strMagSpot;
	BOOL	m_bLowDoseMode;
	BOOL	m_bTieFocusTrial;
	CString	m_strOverlap;
	BOOL	m_bBlankWhenDown;
	CString	m_strPosition;
	BOOL	m_bContinuousUpdate;
	CString	m_strElecDose;
	BOOL	m_bNormalizeBeam;
	CString	m_strBeamShift;
	BOOL	m_bSetBeamShift;
	CString	m_strLDArea;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLowDoseDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLowDoseDlg)
	afx_msg void OnBlankbeam();
	afx_msg void OnLowdosemode();
	afx_msg void OnRdefine();
	afx_msg void OnTiefocustrial();
	virtual BOOL OnInitDialog();
	afx_msg void OnKillfocusEditposition();
	afx_msg void OnBalanceshifts();
	afx_msg void OnCenterunshifted();
	afx_msg void OnContinuousupdate();
	afx_msg void OnUnblank();
	afx_msg void OnLdNormalizeBeam();
	afx_msg void OnResetBeamShift();
	afx_msg void OnSetBeamShift();
  afx_msg void OnPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	afx_msg void OnCopyArea(UINT nID);

private:
	EMimageBuffer *mImBufs;
	CString *mModeNames;
  CString mSearchName;
  CShiftManager *mShiftManager;
  CEMscope *mScope;
	CFont mBigFont;			// Font for bigger things
	BOOL mIgnoreIS;
  BOOL mTrulyLowDose;
  BOOL mHideOffState;
	LowDoseParams *mLDParams;
	int mLastMag;
	int mLastSpot;
	double mLastIntensity;
  double mLastDose;
  float mLastAlpha;
  int mLastProbe;
	double mLastISX, mLastISY;
	double mBaseISX, mBaseISY;      // Base image shift while defining area
  int mBaseMag;                   // Base mag at which that shift applies
	int mLastDefineSize;
	int mLastRecordSize;
	int mLastSetArea;
	int mLastMagIndex;
	BOOL mLastBlanked;
  double mLastBeamX, mLastBeamY;
  double mStartingBeamX, mStartingBeamY;
  double mStartingBTiltX, mStartingBTiltY;
  double mStampViewShift[2];  // Time stamp of image used to set view/search shift
  float mPiezoScaleFac;
  float mPiezoMinToUse;
  float mPiezoMaxToUse;
  int mAxisPiezoPlugNum;    // Plugin number of piezo used for axis
  int mAxisPiezoNum;        // Piezo number
  int mPiezoCurrentSign;    // Current sign of axis position
  float mPiezoFullDelay;     // Delay for full-range movement of piezo, for Record
  float mFocusPiezoDelayFac;  // Amount to cut delay by for focus images
  float mTVPiezoDelayFac;     // Amount to cut it by for trial/focus
  BOOL  mTieFocusTrialPos;   // Flag to keep focus/trial positions together

public:
  GetMember(BOOL, TrulyLowDose);
  int DrawAreaOnView(int type, EMimageBuffer *imBuf, StateParams &state, float * cornerX,
    float * cornerY, float & cenX, float & cenY, float &radius);
  void AreaAcqCoordToView(int inArea, int binning, int sizeX, int sizeY, ScaleMat aMat,
    ScaleMat vMat, int acqX, int acqY, StateParams &state, float & imX, float & imY);
  void ManageDefines(int area);
  CButton m_butBlankBeam;
  void ToggleBlankWhenDown(void);
  CString m_strViewDefocus;
  CSpinButtonCtrl m_sbcViewDefocus;
  afx_msg void OnDeltaposSpinviewdefocus(NMHDR *pNMHDR, LRESULT *pResult);
  CStatic m_statViewDefLabel;
  void CheckSeparationChange(int magIndex);
  CButton m_butSetViewShift;
  CButton m_butZeroViewShift;
  double mViewShiftX[2], mViewShiftY[2];
  afx_msg void OnSetViewShift();
  afx_msg void OnZeroViewShift();
  int OKtoSetViewShift();
  void EnableSetViewShiftIfOK(void);
  CStatic m_statDegrees;
  CEdit m_editAxisAngle;
  int m_iAxisAngle;
  CButton m_butRotateAxis;
  BOOL m_bRotateAxis;
  afx_msg void OnLdRotateAxis();
  afx_msg void OnKillfocusEditAxisangle();
  double ConvertIStoAxisAndAngle(int mag, double ISX, double ISY, int & angle);
  bool ShiftsBalanced(void);
  CStatic m_statViewDefocus;
  void GetNetViewShift(double &shiftX, double &shiftY, int area = VIEW_CONSET);
  void GetEitherViewShift(double &shiftX, double &shiftY, int area = VIEW_CONSET);
  void GetFullViewShift(double &shiftX, double &shiftY, int area = VIEW_CONSET) {shiftX = mViewShiftX[area ? 1 : 0]; shiftY = mViewShiftY[area ? 1 : 0];};
  void CheckAndActivatePiezoShift(void);
  bool GoToPiezoPosForLDarea(int area);
  void ApplyNewISifDefineArea(void);
  int NewAxisPosition(int area, double position, int angle, bool setAngle);
  afx_msg void OnRadioShowOffset();
  int m_iOffsetShown;
  void SyncFocusAndTrial(int fromArea);
  void SetTieFocusTrialPos(BOOL inVal);
  void SyncPosOfFocusAndTrial(int fromArea);
  void DeselectGoToButtons(int area);
  void SelectGoToButton(int area);
  CButton m_butCopyToSearch;
  afx_msg void OnGotoArea(UINT nID);
  CMyButton m_butGotoView;
  CMyButton m_butGotoFocus;
  CMyButton m_butGotoTrial;
  CMyButton m_butGotoSearch;
  CMyButton m_butGotoRecord;
  void SetBeamShiftButton(BOOL state);
  bool UsableDefineImageInAOrView(EMimageBuffer *imBuf);
  bool ViewImageOKForEditingFocus(EMimageBuffer * imBuf);
  int m_iGoToArea;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LOWDOSEDLG_H__7DD4D5B2_CBF6_4DD3_902E_EAF16F469071__INCLUDED_)
