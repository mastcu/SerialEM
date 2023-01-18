#if !defined(AFX_MENUTARGETS_H__67FD80A8_52D8_4C40_B719_ECBD88387C2F__INCLUDED_)
#define AFX_MENUTARGETS_H__67FD80A8_52D8_4C40_B719_ECBD88387C2F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MenuTargets.h : header file
//

class CNavigatorDlg;
class CCameraController;
class CEMscope;
class CShiftManager;
class CTSController;
class CNavHelper;
class CScreenShotDialog;

/////////////////////////////////////////////////////////////////////////////
// CMenuTargets command target

class CMenuTargets : public CCmdTarget
{

  //DECLARE_DYNCREATE(CMenuTargets)
public:
	CMenuTargets();           // protected constructor used by dynamic creation
  ~CMenuTargets();
// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMenuTargets)
	//}}AFX_VIRTUAL

// Implementation
protected:
//	virtual ~CMenuTargets();

	// Generated message map functions
	//{{AFX_MSG(CMenuTargets)
	afx_msg void OnUpdateLoadNavFile(CCmdUI* pCmdUI);
	afx_msg void OnLoadNavFile();
  afx_msg void OnTasksNavigator();
	afx_msg void OnUpdateTasksNavigator(CCmdUI* pCmdUI);
	afx_msg void OnSaveNavFile();
	afx_msg void OnUpdateSaveNavFile(CCmdUI* pCmdUI);
	afx_msg void OnSaveasNavFile();
	afx_msg void OnNavigatorClose();
	afx_msg void OnUpdateNavigatorClose(CCmdUI* pCmdUI);
	afx_msg void OnCornerMontage();
	afx_msg void OnUpdateCornerMontage(CCmdUI* pCmdUI);
	afx_msg void OnPolygonMontage();
	afx_msg void OnUpdatePolygonMontage(CCmdUI* pCmdUI);
	afx_msg void OnNavigatorAcquire();
	afx_msg void OnUpdateNavigatorAcquire(CCmdUI* pCmdUI);
	afx_msg void OnNewMap();
	afx_msg void OnUpdateNewMap(CCmdUI* pCmdUI);
	afx_msg void OnTransformPts();
	afx_msg void OnUpdateTransformPts(CCmdUI* pCmdUI);
	afx_msg void OnDeleteitem();
	afx_msg void OnUpdateDeleteitem(CCmdUI* pCmdUI);
	afx_msg void OnCalibrationDistortion();
  afx_msg void OnUpdateNoTasks(CCmdUI* pCmdUI);
  afx_msg void OnUpdateNoTasksNoTS(CCmdUI* pCmdUI);
  afx_msg void OnCalibrationMagenergyshifts();
  afx_msg void OnUpdateCalibrationMagenergyshifts(CCmdUI* pCmdUI);
  afx_msg void OnAcquiregainref();
  afx_msg void OnUpdateAcquiregainref(CCmdUI* pCmdUI);
  afx_msg void OnCalibrationBeamintensity();
  afx_msg void OnUpdateCalibrationBeamintensity(CCmdUI* pCmdUI);
  afx_msg void OnCalibrationBeamshift();
  afx_msg void OnUpdateCalibrationBeamshift(CCmdUI* pCmdUI);
  afx_msg void OnCalibrationImageshift();
	afx_msg void OnCalibrationStageshift();
  afx_msg void OnCalibrationIsfromscratch();
  afx_msg void OnAutoalign();
  afx_msg void OnTiltdown();
  afx_msg void OnUpdateTiltdown(CCmdUI* pCmdUI);
  afx_msg void OnTiltup();
	afx_msg void OnCalibrationListmags();
  afx_msg void OnUpdateCalibrationListmags(CCmdUI* pCmdUI);
  afx_msg void OnCameraStopacquire();
  afx_msg void OnUpdateCameraStopacquire(CCmdUI* pCmdUI);
  afx_msg void OnCameraView();
  afx_msg void OnCameraFocus();
  afx_msg void OnCameraTrial();
  afx_msg void OnCameraRecord();
  afx_msg void OnCameraPreview();
  afx_msg void OnCameraScreendown();
  afx_msg void OnUpdateCameraScreendown(CCmdUI* pCmdUI);
  afx_msg void OnCameraMontage();
  afx_msg void OnUpdateCameraMontage(CCmdUI* pCmdUI);
  afx_msg void OnCameraMontagetrial();
  afx_msg void OnUpdateCameraMontagetrial(CCmdUI* pCmdUI);
  afx_msg void OnCameraSettiming();
  afx_msg void OnUpdateCameraSettiming(CCmdUI* pCmdUI);
  afx_msg void OnCameraDebugmode();
  afx_msg void OnUpdateCameraDebugmode(CCmdUI* pCmdUI);
  afx_msg void OnCameraShowgainref();
  afx_msg void OnUpdateCameraShowref(CCmdUI* pCmdUI);
  afx_msg void OnCameraShowdarkref();
  afx_msg void OnCameratoggle();
  afx_msg void OnpdateCameratoggle(CCmdUI* pCmdUI);
  afx_msg void OnEscape();
  afx_msg void OnUpdateEscape(CCmdUI* pCmdUI);
  afx_msg void OnCameraPostactions();
  afx_msg void OnUpdateCameraPostactions(CCmdUI* pCmdUI);
	afx_msg void OnCalibrationElectrondose();
	afx_msg void OnCalibrationC2factor();
  afx_msg void OnTasksTiltseries();
  afx_msg void OnUpdateTasksTiltseries(CCmdUI* pCmdUI);
  afx_msg void OnTiltseriesBackup();
  afx_msg void OnTiltseriesStop();
  afx_msg void OnUpdateTiltseriesStop(CCmdUI* pCmdUI);
  afx_msg void OnTiltseriesEndloop();
  afx_msg void OnTiltseriesResume();
  afx_msg void OnUpdateTiltseriesResume(CCmdUI* pCmdUI);
  afx_msg void OnTiltseriesOneloop();
  afx_msg void OnTiltseriesOnestep();
  afx_msg void OnTiltseriesTerminate();
  afx_msg void OnUpdateTiltseriesBackup(CCmdUI* pCmdUI);
  afx_msg void OnTiltseriesVerbose();
  afx_msg void OnUpdateTiltseriesVerbose(CCmdUI* pCmdUI);
	afx_msg void OnTiltseriesDebugmode();
	afx_msg void OnUpdateTiltseriesDebugmode(CCmdUI* pCmdUI);
	afx_msg void OnTiltseriesAutosavelog();
	afx_msg void OnUpdateTiltseriesAutosavelog(CCmdUI* pCmdUI);
	afx_msg void OnCalibrationBeamcrossover();
	afx_msg void OnCalibrationSetoverlaps();
	afx_msg void OnNavigatorAutosave();
	afx_msg void OnUpdateNavigatorAutosave(CCmdUI* pCmdUI);
	afx_msg void OnNavigatorSetupfullmontage();
	afx_msg void OnUpdateNavigatorSetupfullmontage(CCmdUI* pCmdUI);
	afx_msg void OnCameraDivideby2();
	afx_msg void OnUpdateCameraDivideby2(CCmdUI* pCmdUI);
	afx_msg void OnCalibrationCameratiming();
	afx_msg void OnCameraNormalizehere();
	afx_msg void OnUpdateCameraNormalizehere(CCmdUI* pCmdUI);
	afx_msg void OnCameraSetcorrections();
	afx_msg void OnUpdateCameraSetcorrections(CCmdUI* pCmdUI);
	afx_msg void OnNavigatorConvertmaps();
	afx_msg void OnUpdateNavigatorConvertmaps(CCmdUI* pCmdUI);
	afx_msg void OnCameraSetscanning();
	afx_msg void OnCalibrationNeutralISvalues();
	afx_msg void OnCalibrationRestoreNeutralIS();
	afx_msg void OnCalibrationBaseFocusValues();
	afx_msg void OnUpdateCalibrationNeutralISvalues(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCalibrationRestoreNeutralIS(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCalibrationC2factor(CCmdUI* pCmdUI);
	afx_msg void OnCalibrationSpotintensity();
	afx_msg void OnCalibrationPurgeoldcalibs();
	afx_msg void OnNavigatorSkewedsupermontage();
	afx_msg void OnUpdateNavigatorSkewedsupermontage(CCmdUI* pCmdUI);
	afx_msg void OnNavigatorAlignedsupermontage();
	afx_msg void OnCalibrationListisvectors();
	afx_msg void OnCalibrationListstagecals();
  afx_msg void OnCalibrationShutterdeadtime();
  afx_msg void OnUpdateCalibrationShutterdeadtime(CCmdUI *pCmdUI);
  afx_msg void OnCalibrationSetdebugoutput();
	afx_msg void OnTiltseriesExtraoutput();
	afx_msg void OnCalibrationEftemISoffset();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

public:
	void Initialize();
  BOOL DoingTasks();
  CNavigatorDlg *mNavigator;
  int SetTwoFlags(CString mess1, CString mess2, int flag1, int flag2, int &value);
  void DoListStageCals();
  void DoListISVectors(BOOL useCalPixel);
  void SaveNextISVectorsForGraph(int inCam, int whichSave) {
    mCamForNextVectorSave = inCam; mWhichNextVectorSave = whichSave;
  };

private:
  CSerialEMApp *mWinApp;
  CCameraController *mCamera;
  CEMscope *mScope;
  CShiftManager *mShiftManager;
  CTSController *mTSController;
  CNavHelper *mNavHelper;
  CString mFlybackSet;
  CScreenShotDialog* m_pDlgScreenShot;
  int mCamForNextVectorSave;
  int mWhichNextVectorSave;

public:
  afx_msg void OnNavigatorChangeregistration();
  afx_msg void OnUpdateNavigatorChangeregistration(CCmdUI *pCmdUI);
  afx_msg void OnUpdateNoUserShots(CCmdUI* pCmdUI);
  afx_msg void OnCalibrationMagisoffsets();
  afx_msg void OnUpdateCalibrationEftemISoffset(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorRotatemap();
  afx_msg void OnUpdateNavigatorRotatemap(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorAligntoitem();
  afx_msg void OnUpdateNavigatorAligntoitem(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorShiftbylasterror();
  afx_msg void OnUpdateNavigatorShiftbylasterror(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorShifttomarker();
  afx_msg void OnUpdateNavigatorShifttomarker(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorUndolastshift();
  afx_msg void OnUpdateNavigatorUndolastshift(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorLoadunbinned();
  afx_msg void OnUpdateNavigatorLoadunbinned(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorSetgridlimits();
  afx_msg void OnCalibrationStandardLMfocus();
  afx_msg void OnCameraGainrefpolicy();
  afx_msg void OnTiltseriesResetpoleangle();
  afx_msg void OnTiltseriesFuturepolicies();
  afx_msg void OnNavigatorEndacquire();
  afx_msg void OnUpdateNavigatorEndacquire(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorSetacquirestate();
  afx_msg void OnNavigatorRestorestate();
  afx_msg void OnUpdateNavigatorSetacquirestate(CCmdUI *pCmdUI);
  afx_msg void OnUpdateNavigatorRestorestate(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorAddcirclepolygon();
  afx_msg void OnUpdateNavigatorAddCirclePolygon(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorDeleteGroup();
  afx_msg void OnUpdateNavigatorDeleteGroup(CCmdUI *pCmdUI);
  afx_msg void OnPolygonsupermontage();
  afx_msg void OnUpdatePolygonsupermontage(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorImportmap();
  afx_msg void OnNavigatorUndotransformation();
  afx_msg void OnUpdateNavigatorUndotransformation(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorAddGridOfPoints();
  afx_msg void OnUpdateNavigatorAddGridOfPoints(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorSetGridGroupSize();
  afx_msg void OnNavigatorDivideIntoGroups();
  afx_msg void OnUpdateNavigatorDivideIntoGroups(CCmdUI *pCmdUI);
  afx_msg void OnMontageListFilesToOpen();
  afx_msg void OnCameraSetcountlimit();
  afx_msg void OnTasksSetupcooker();
  afx_msg void OnTasksCookspecimen();
  afx_msg void OnTasksSetupAutocenter();
  afx_msg void OnUpdateTasksSetupAutocenter(CCmdUI *pCmdUI);
  afx_msg void OnTasksAutocenterBeam();
  afx_msg void OnUpdateTasksAutocenterbeam(CCmdUI *pCmdUI);
  afx_msg void OnCameraSetdriftinterval();
  afx_msg void OnTiltseriesSendemail();
  afx_msg void OnUpdateTiltseriesSendemail(CCmdUI *pCmdUI);
  afx_msg void OnTiltseriesSetaddress();
  afx_msg void OnTasksAssessAngleRange();
  afx_msg void OnNavigatorOpenStateDlg();
  afx_msg void OnUpdateNavigatorOpenStateDlg(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorGroupAcquire();
  afx_msg void OnUpdateNavigatorGroupAcquire(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorAlignToMap();
  afx_msg void OnUpdateNavigatorAlignToMap(CCmdUI *pCmdUI);
  afx_msg void OnCameraInterpolateDarkRefs();
  afx_msg void OnUpdateCameraInterpolateDarkRefs(CCmdUI *pCmdUI);
  afx_msg void OnCalibrationStagestretch();
  afx_msg void OnUpdateCalibrationStagestretch(CCmdUI *pCmdUI);
  afx_msg void OnUpdateCameraGainrefpolicy(CCmdUI *pCmdUI);
  afx_msg void OnCalibrationSetIsDelayFactor();
  afx_msg void OnNavigatorPolygonFromCorners();
  afx_msg void OnUpdateNoTasksNoSTEM(CCmdUI *pCmdUI);
  afx_msg void OnUpdateNoTasksNoTSnoSTEM(CCmdUI *pCmdUI);
  afx_msg void OnTasksAssessIntersetShifts();
  afx_msg void OnUpdateTasksAssessIntersetShifts(CCmdUI *pCmdUI);
  afx_msg void OnTasksReviseCancel();
  afx_msg void OnUpdateTasksReviseCancel(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorRealignScaling();
  afx_msg void OnUpdateNavigatorRealignScaling(CCmdUI *pCmdUI);
  afx_msg void OnCalibrationQuickFlybackTime();
  afx_msg void OnUpdateCalibrationQuickFlybackTime(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorAdjustBacklash();
  afx_msg void OnUpdateNavigatorAdjustBacklash(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorBacklashSettings();
  afx_msg void OnCalibrationAlphaBeamShifts();
  afx_msg void OnUpdateCalibrationAlphaBeamShifts(CCmdUI *pCmdUI);
  afx_msg void OnCalibrationSetApertureSize();
  afx_msg void OnUpdateCalibrationSetApertureSize(CCmdUI *pCmdUI);
  afx_msg void OnCalibrationWholeImageCorr();
  afx_msg void OnUpdateCalibrationWholeImageCorr(CCmdUI *pCmdUI);
  afx_msg void OnCalibrationSetCycleLengths();
  int DoCalibrateStandardLMfocus(bool fromMacro);
  afx_msg void OnTiltseriesCallTsPlugin();
  afx_msg void OnUpdateTiltseriesCallTsPlugin(CCmdUI *pCmdUI);
  afx_msg void OnSpecialSkipBeamShiftInTs();
  afx_msg void OnUpdateSpecialSkipBeamShiftInTs(CCmdUI *pCmdUI);
  afx_msg void OnSpecialPiezoForLdAxisShift();
  afx_msg void OnUpdateSpecialPiezoForLdAxisShift(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorMergefile();
  afx_msg void OnUpdateNavigatorMergefile(CCmdUI *pCmdUI);
  afx_msg void OnCameraSetQuality();
  afx_msg void OnUpdateCameraSetQuality(CCmdUI *pCmdUI);
  afx_msg void OnWindowStageMoveTool();
  afx_msg void OnNavigatorForceCenterAlign();
  afx_msg void OnSpecialDisableDarkTrimInAlign();
  afx_msg void OnCalibrationSpotBeamShifts();
  afx_msg void OnAllowContinuousInTS();
  afx_msg void OnUpdateAllowContinuousInTS(CCmdUI *pCmdUI);
  afx_msg void OnUseTrialBinning();
  afx_msg void OnUseTrialSize();
  afx_msg void OnUpdateUseTrialBinning(CCmdUI *pCmdUI);
  afx_msg void OnUpdateUseTrialSize(CCmdUI *pCmdUI);
  afx_msg void OnRemoveIScal();
  afx_msg void OnRemoveStageCal();
  void RemoveOneCal(bool doStage);
  afx_msg void OnAdjustFocusOnProbeModeChange();
  afx_msg void OnUpdateAdjustFocusOnProbeModeChange(CCmdUI *pCmdUI);
  afx_msg void OnTiltseriesSaveXYZFile();
  afx_msg void OnUpdateTiltseriesSaveXYZFile(CCmdUI *pCmdUI);
  afx_msg void OnCameraNoNormOfDSdoseFrac();
  afx_msg void OnUpdateCameraNoNormOfDSdoseFrac(CCmdUI *pCmdUI);
  afx_msg void OnTiltseriesAutosaveXYZ();
  afx_msg void OnUpdateTiltseriesAutosaveXYZ(CCmdUI *pCmdUI);
  afx_msg void OnFocusCorrectAstigmatism();
  afx_msg void OnUpdateFocusCorrectAstigmatism(CCmdUI *pCmdUI);
  afx_msg void OnFocusComaFree();
  afx_msg void OnUpdateFocusComaFree(CCmdUI *pCmdUI);
  afx_msg void OnUpdateCalibrateAstigmatism(CCmdUI *pCmdUI);
  afx_msg void OnCalibrateComaFree();
  afx_msg void OnUpdateCalibrateComaFree(CCmdUI *pCmdUI);
  afx_msg void OnCalibrateAstigmatism();
  bool CamToSpecimenExists(void);
  afx_msg void OnCalibrateSetupComaFree();
  afx_msg void OnCalibrateSetupAstigmatism();
  afx_msg void OnFocusSetAstigBeamTilt();
  afx_msg void OnFocusSetComaBeamTilt();
  afx_msg void OnTiltseriesRunMacro();
  afx_msg void OnUpdateTiltseriesRunMacro(CCmdUI *pCmdUI);
  afx_msg void OnTiltseriesSetMacroToRun();
  afx_msg void OnMontageUseFilterSet2();
  afx_msg void OnUpdateMontageUseFilterSet2(CCmdUI *pCmdUI);
  afx_msg void OnMontageUseSet2InLowDose();
  afx_msg void OnUpdateMontageUseSet2InLowDose(CCmdUI *pCmdUI);
  afx_msg void OnMontageDivideSet1By2();
  afx_msg void OnUpdateMontageDivideSet1By2(CCmdUI *pCmdUI);
  afx_msg void OnMontageDivideSet2By2();
  afx_msg void OnUpdateMontageDivideSet2By2(CCmdUI *pCmdUI);
  afx_msg void OnMontageTestSet1Values();
  afx_msg void OnMontageTestSet2Values();
  afx_msg void OnMontageRedoCorrWhenRead();
  afx_msg void OnUpdateMontageRedoCorrWhenRead(CCmdUI *pCmdUI);
  afx_msg void OnCalibrationHighDefocus();
  afx_msg void OnListFocusMagCals();
  afx_msg void OnTiltseriesFixedNumFrames();
  afx_msg void OnUpdateTiltseriesFixedNumFrames(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorWriteAsXmlFile();
  afx_msg void OnUpdateNavigatorWriteAsXmlFile(CCmdUI *pCmdUI);
  afx_msg void OnSetPointLabelThreshold();
  afx_msg void OnCameraSetFrameAlignLines();
  afx_msg void OnSettingsSetProperty();
  afx_msg void OnFocusSetComaIterations();
  afx_msg void OnFocusRunAnotherIteration();
  afx_msg void OnUpdateFocusRunAnotherIteration(CCmdUI *pCmdUI);
  afx_msg void OnFocusMakeZemlinTableau();
  afx_msg void OnNormalizeAllLensesOnMag();
  afx_msg void OnUseCurrentLDparamsInNavRealign();
  afx_msg void OnUpdateUseCurrentLDparamsInNavRealign(CCmdUI *pCmdUI);
  afx_msg void OnCameraUseViewForSearch();
  afx_msg void OnUpdateCameraUseViewForSearch(CCmdUI *pCmdUI);
  afx_msg void OnCameraUseRecordForMontage();
  afx_msg void OnUpdateCameraUseRecordForMontage(CCmdUI *pCmdUI);
  afx_msg void OnCameraSearch();
  afx_msg void OnNavigatorShowMultiShot();
  afx_msg void OnNavigatorSetMultiShotParams();
  afx_msg void OnUpdateNavigatorShowMultiShot(CCmdUI *pCmdUI);
  afx_msg void OnCalFocusTuningCtfAstig();
  afx_msg void OnFocusComaByCtf();
  afx_msg void OnFocusSetCtfAcquireParams();
  afx_msg void OnFocusCorrectAstigmatismWithFfts();
  afx_msg void OnFocusSetCtfComaBt();
  afx_msg void OnUpdateFocusCorrectAstigmatismWithFfts(CCmdUI *pCmdUI);
  afx_msg void OnCalibrateComaVsIS();
  afx_msg void OnShowWholeAreaForAllPoints();
  afx_msg void OnUpdateShowWholeAreaForAllPoints(CCmdUI *pCmdUI);
  afx_msg void OnMontagingGridsAddGridLikeLastOne();
  afx_msg void OnUpdateOnMontagingGridsAddGridLikeLastOne(CCmdUI *pCmdUI);
  afx_msg void OnExternalTool(UINT nID);
  void OpenNavigatorIfClosed(void);
  afx_msg void OnCameraAlwaysAntialiasK23();
  afx_msg void OnUpdateCameraAlwaysAntialiasK23(CCmdUI *pCmdUI);
  afx_msg void OnBeamSpotListCalibrations();
  afx_msg void OnTasksSetupWaitForDrift();
  afx_msg void OnWindowTakeScreenShot();  
  afx_msg void OnSkipBlankingInLdWithScreenUp();
  afx_msg void OnUpdateSkipBlankingInLdWithScreenUp(CCmdUI *pCmdUI);
  afx_msg void OnTasksSetupVppConditioning();
  afx_msg void OnUpdateTasksSetupVppConditioning(CCmdUI *pCmdUI);
  afx_msg void OnMontagingGridsFindHoles();
  afx_msg void OnUpdateMontagingGridsFindHoles(CCmdUI *pCmdUI);
  afx_msg void OnCombinePointsIntoMultiShots();
  afx_msg void OnCameraSetExtraDivisionBy2();
  afx_msg void OnUpdateCameraSetExtraDivisionBy2(CCmdUI *pCmdUI);
  afx_msg void OnCameraReturnFloatImage();
  afx_msg void OnUpdateCameraReturnFloatImage(CCmdUI *pCmdUI);
  afx_msg void OnBeamspotRefineBeamShift();
  afx_msg void OnUpdateBeamspotRefineBeamShift(CCmdUI *pCmdUI);
  afx_msg void OnBeamspotIlluminatedAreaLimits();
  afx_msg void OnUpdateIlluminatedAreaLimits(CCmdUI *pCmdUI);
  afx_msg void OnOptionsSearchPlusMinus();
  afx_msg void OnUpdateOptionsSearchPlusMinus(CCmdUI *pCmdUI);
  afx_msg void OnMarkerToCenter();
  afx_msg void OnUseItemLabelsInFilenames();
  afx_msg void OnUpdateUseItemLabelsInFilenames(CCmdUI *pCmdUI);
  afx_msg void OnCloseValvesAfterLongInactivity();
  afx_msg void OnUpdateCloseValvesAfterLongInactivity(CCmdUI *pCmdUI);
  afx_msg void OnNavigatorApplySavedShift();
  afx_msg void OnUpdateNavigatorApplySavedShift(CCmdUI *pCmdUI);
  afx_msg void OnEucentricityByFocus();
  afx_msg void OnUpdateEucentricityByFocus(CCmdUI *pCmdUI);
  afx_msg void OnSetupEucentricityByFocus();
  afx_msg void OnNavigatorSetupAlign();
  afx_msg void OnUpdateNavigatorSetupAlign(CCmdUI *pCmdUI);
  afx_msg void OnSetupScopeManagement();
  afx_msg void OnUpdateSetupScopeManagement(CCmdUI *pCmdUI);
  afx_msg void OnTasksUseViewEvenIfSearchBetter();
  afx_msg void OnUpdateTasksUseViewEvenIfSearchBetter(CCmdUI *pCmdUI);
  afx_msg void OnKeepFocusTrialAtSamePosition();
  afx_msg void OnUpdateKeepFocusTrialAtSamePosition(CCmdUI *pCmdUI);
  afx_msg void OnTiltseriesSetBidirReturnDelay();
  afx_msg void OnSetParamsForGridMap();
  afx_msg void OnAutocontourGridSquares();
  afx_msg void OnReverseContourColors();
  afx_msg void OnUpdateReverseContourColors(CCmdUI *pCmdUI);
  afx_msg void OnKeepColorsForPolygons();
  afx_msg void OnUpdateKeepColorsForPolygons(CCmdUI *pCmdUI);
  afx_msg void OnCalHighDefocusIS();
  afx_msg void OnSetHoleOrderForRegularArray();
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MENUTARGETS_H__67FD80A8_52D8_4C40_B719_ECBD88387C2F__INCLUDED_)
