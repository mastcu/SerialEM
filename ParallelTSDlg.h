#pragma once
#include "afxwin.h"
#include "afxcmn.h"
#include "NavHelper.h"

#define PARALLELTSDLG_NUM_PANELS 7
#define FLAG_DISPLAY_EXTRA_OPTS   (1)

// CParallelTSDlg dialog

class CParallelTSDlg : public CBaseDlg
{

public:
  CParallelTSDlg(CWnd* pParent = NULL);   // standard constructor
  virtual ~CParallelTSDlg();
  bool IsOpen() { return mIsOpen; };

  // Dialog Data
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_PARALLELTS };
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
  CShiftManager *mShiftManager;
  CNavHelper *mNavHelper;
  CParallelTSHelper *mParallelTSHelper;
  ParallelTSOptions *mParTSopts;
  ParallelTSOptions mSavedParTSopts;

  int mPanelStart[PARALLELTSDLG_NUM_PANELS];
  int mNumInPanel[PARALLELTSDLG_NUM_PANELS];
  bool mDisplayExtraOptions;
  bool mIsOpen;
  int mMapMagIndex;
  int mAcqMagIndex;
  int mSTEMindex;
  int mHasIlluminatedArea;
  int mCurrentCamera;
  int mNumberBeforeAdd;
  int mFitPlaneGroupID;
  bool mDefiningPoints;               // Flag if defining points to find pretilt, X pitch
  bool mSettingUpTargetArea;        // Flag if setting up a new target area
  bool mHasAreaMap;                   // Flag if the area has a map
  int mTargetGroupID;
  bool mAddingTargets;                // Flag if targets are being added to area
  bool mRefiningTargets;              // Flag if targets are being refined
  bool mFinishedRefiningTargets;      // Flag for when a round of targets have been refined
  bool mFinalizedTargetArea;          // Flag if area and targets are finalized
  int mSavedTSparamIndex;
  bool mDrawingISTargets;
  int mNumAddedTargets;
  
  void ClearArea();
  void CancelAddingDefining();
  bool KeepAddingChoiceBox(CString mess);

public:
  GetMember(int, FitPlaneGroupID);
  GetMember(int, TargetGroupID);
  GetMember(bool, DrawingISTargets);
  GetMember(int, MapMagIndex);
  GetMember(int, AcqMagIndex);
  GetSetMember(int, HasIlluminatedArea);
  GetMember(bool, SettingUpTargetArea);
  void IncrementNumTargetsAdded() { mNumAddedTargets++; };
  bool IsDefiningPoints() { return mDefiningPoints; };
  bool IsAddingTargets() { return mAddingTargets; };
  void FinishFitPlane();
  void FinishRefineTargets(bool error);
  void UpdatePlaneParams(float pretilt, float xPitchAngle);
  bool HasAreaMap() { return mHasAreaMap; };
  bool RefiningTargets() { return mRefiningTargets; };
  bool IsAddingToNav() { return mDefiningPoints || mAddingTargets; };
  bool AreaMapInBuf(EMimageBuffer *imBuf);
  void DoPlaneFit();
  void StartRefineTargets();
  void CloseWindow();
  CStatic m_statMappingMag;
  CSpinButtonCtrl m_sbcMappingMag;
  afx_msg void OnDeltaposSpinPtsMapmag(NMHDR *pNMHDR, LRESULT *pResult);
  CStatic m_statAcquisitionMag;
  CSpinButtonCtrl m_sbcAcquisitionMag;
  afx_msg void OnDeltaposSpinPtsAcqmag(NMHDR *pNMHDR, LRESULT *pResult);
  CString m_strMappingMag;
  CString m_strAcquisitionMag;
  CFont* mBoldFont;
  CFont* mRegFont;
  CStatic m_statSpecimenPretiltAngle;
  CButton m_butDefinePtsFitPlane;
  afx_msg void OnDefinePtsFitPlane();
  float m_fPretilt;
  afx_msg void OnEnKillfocusEditPretilt();
  float m_fXpitch;
  afx_msg void OnEnKillfocusEditXpitch();
  CStatic m_statDefineAreaTargets;
  int m_iCustomOrMultishotTargets;
  CButton m_butAddCustomTargets;
  CButton m_butAddMultishotItem;
  afx_msg void OnRadioCustomTargets();
  afx_msg void OnRadioMultishotTargets();
  afx_msg void OnRoughEucen();
  afx_msg void OnEucenByFocus();
  afx_msg void OnStartNewTargetArea();
  afx_msg void OnLDSearchElseView();
  afx_msg void OnLDViewElseTrial();
  afx_msg void OnPreview();
  afx_msg void OnSaveAreaMap();
  CButton m_butAddTargets;
  CButton m_butRefineTargets;
  afx_msg void OnAddTargets();
  afx_msg void OnSaveTargetMap();
  afx_msg void OnRemoveTarget();
  afx_msg void OnFinalizeArea();
  afx_msg void OnAbortArea();
  CString m_strTSItemIndexLabel;
  afx_msg void OnSetupTiltSeries();
  CButton m_butOpenCloseOptions;
  afx_msg void OnOpenCloseOptions();
  CStatic m_statAcqDisplayOptions;
  float m_fMaxTilt;
  afx_msg void OnEnKillfocusEditMaxtilt();
  float m_fBeamDiameter;
  afx_msg void OnEnKillfocusEditDiam();
  CButton m_butBeamSizeCircles;
  BOOL m_bBeamSizeCircles;
  float m_fExtraDelayFactor;
  BOOL m_bAdjustBeamTiltAstig;
  int m_iCTFType;
  BOOL m_bFindAstig;
  CButton m_butSaveTargetMap;
  CButton m_butRemoveTarget;
  CButton m_butFinalizeTargetArea;
  CButton m_butAbortArea;
  CButton m_butApplyAdjusting;
  BOOL m_bApplyAdjusting;
  void Update();
  void ManagePanels();
  afx_msg void OnCtf();
  CStatic m_statMapping;
  CStatic m_statAcquisition;
  afx_msg void OnMontage();
  afx_msg void OnRadioAlignRef();
  float m_fRefMaxRotation;
  float m_fRefMaxScaling;
  int m_iAlignRef;
  bool IsDoingNewArea() { return mSettingUpTargetArea; }
  void ExternalUpdate();
  void OptionsToDialog();
  void DialogToOptions();

  CStatic m_statMagFor;
  CEdit m_butPretilt;
  CEdit m_butXpitch;
  CButton m_butRoughEucen;
  CButton m_butEucenByFocus;
  CButton m_butStartNewTargetArea;
  CButton m_butLDSearchElseView;
  CButton m_butLDViewElseTrial;
  CButton m_butPreview;
  CButton m_butMontage;
  CButton m_butSaveAreaMap;
  CButton m_butAlignPreview;
  CButton m_butSetupTiltSeries;
  CEdit m_butMaxTilt;
  CEdit m_butDiameter;
  CEdit m_butExtraISDelay;
  CButton m_butBeamTiltAstig;
  CButton m_butCTFnone;
  CButton m_butFindAstig;
  CEdit m_butMaxRotation;
  CEdit m_butMaxScaling;
  CStatic m_statPretilt;
  CStatic m_statPretiltDeg;
  CStatic m_statXpitchAngle;
  CStatic m_statXpitchAngleDeg;
  CStatic m_statAlignStartingTilt;
  CStatic m_statAdjustingXformStatus;
  CStatic m_statInheritingTSparams;
  CStatic m_statTSitemLabel;
  float m_fMaxAlignShift;
  afx_msg void OnApplyadjusting();
  afx_msg void OnBeamsizecircles();
  afx_msg void OnEnKillfocusEditExtradelay();
  afx_msg void OnAdjustbeamtilt();
  afx_msg void OnFindastig();
  afx_msg void OnEnKillfocusEditMaxalignshift();
  afx_msg void OnEnKillfocusEditMaxrotation();
  afx_msg void OnEnKillfocusEditMaxscaling();
  CStatic m_statAreaMapStatus;
  afx_msg void OnRefineTargets();
};
