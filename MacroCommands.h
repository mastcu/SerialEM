#ifndef MACROCOMMANDS_H
#define MACROCOMMANDS_H
#include "MacroProcessor.h"
#include "EMscope.h"
class CMacCmd : public CMacroProcessor
{
 public:
  CMacCmd();
  virtual ~CMacCmd();
  int NextCommand(bool startingOut);
  void TaskDone(int param);

 private:

  // Member variables assigned to in NextCommand
  static const char *mLongKeys[MAX_LONG_OPERATIONS];
  static int mLongHasTime[MAX_LONG_OPERATIONS];


  CString mStrLine, mStrCopy, mItem1upper;
  CString mStrItems[MAX_MACRO_TOKENS];
  CString *mMacro;

  // Use this only for output followed by break not return, output at end of switch
  CString mLogRpt;
  BOOL mItemEmpty[MAX_MACRO_TOKENS];
  int mItemInt[MAX_MACRO_TOKENS];
  double mItemDbl[MAX_MACRO_TOKENS];
  float mItemFlt[MAX_MACRO_TOKENS];
  int mCmdIndex, mLastNonEmptyInd;
  BOOL mKeyBreak;
  int mCurrentCam;
  int mReadOtherSleep;
  FilterParams *mFiltParam;
  int *mActiveList;
  CameraParameters *mCamParams;
  MontParam *mMontP;
  LowDoseParams *mLdParam;
  CNavigatorDlg *mNavigator;

  // "Convenience" member variables that can be replaced with local variables in functions
  CString cReport;
  BOOL cTruth, cDoShift, cDoPause, cDoAbort;
  bool cDoBack;
  ScaleMat cAMat, cBInv;
  EMimageBuffer *cImBuf;
  KImage *cImage;
  CFile *cCfile;
  double cDelISX, cDelISY, cDelX, cDelY, cH1, cV1, cV2, cH2, cH3, cV3, cV4, cH4;
  double cStageX, cStageY, cStageZ, cSpecDist;
  float cFloatX, cFloatY;
  int cIndex, cIndex2, cI, cIx0, cIx1, cIy0, cIy1, cSizeX, cSizeY, cMag;
  int cBinning;
  float cBacklashX, cBacklashY, cBmin, cBmax, cBmean, cBSD, cCpe, cShiftX, cShiftY;
  float cFitErr;
  CMapDrawItem *cNavItem;
  MacroFunction *cFunc;
  Variable *cVar;
  CString *cValPtr;
  int *cNumElemPtr;
  CFileStatus cStatus;
  FileForText *cTxFile;
  StageMoveInfo cSmi;

 public:
  // COMMAND FUNCTIONS
  int ScriptEnd(void);
  int Repeat(void);
  int NoOperation(void);
  int EndLoop(void);
  int Loop(void);
  int If(void);
  int Endif(void);
  int Else(void);
  int Break(void);
  int Try(void);
  int Catch(void);
  int EndTry(void);
  int Throw(void);
  int SkipTo(void);
  int DoMacro(void);
  int ParseQuotedStrings(void);
  int StringArrayToScript(void);
  int OnStopCallFunc(void);
  int NoMessageBoxOnError(void);
  int Test(void);
  int SetVariableCmd(void);
  int TruncateArray(void);
  int ArrayStatistics(void);
  int LinearFitToVars(void);
  int SetStringVar(void);
  int ListVars(void);
  int ListPersistentVars(void);
  int ClearPersistentVars(void);
  int MakeVarPersistent(void);
  int IsVariableDefined(void);
  int NewArrayCmd(void);
  int LocalVar(void);
  int LocalLoopIndexes(void);
  int Plugin(void);
  int ListPluginCalls(void);
  int FlashDisplay(void);
  int TiltUp(void);
  int TiltDown(void);
  int TiltTo(void);
  int SetStageBAxis(void);
  int OpenDECameraCover(void);
  int View(void);
  int Focus(void);
  int Trial(void);
  int Record(void);
  int Preview(void);
  int Search(void);
  int Montage(void);
  int OppositeTrial(void);
  int AcquireToMatchBuffer(void);
  int StepFocusNextShot(void);
  int SmoothFocusNextShot(void);
  int DeferStackingNextShot(void);
  int EarlyReturnNextShot(void);
  int GetDeferredSum(void);
  int FrameThresholdNextShot(void);
  int QueueFrameTiltSeries(void);
  int SetFrameSeriesParams(void);
  int WriteFrameSeriesAngles(void);
  int ReportFrameTiltSum(void);
  int ModifyFrameTSShifts(void);
  int ReplaceFrameTSFocus(void);
  int RetractCamera(void);
  int RecordAndTiltUp(void);
  int ArePostActionsEnabled(void);
  int TiltDuringRecord(void);
  int TestNextMultiShot(void);
  int MultipleRecords(void);
  int RotateMultiShotPattern(void);
  int AutoAlign(void);
  int LimitNextAutoAlign(void);
  int AlignWithRotation(void);
  int AutoFocus(void);
  int BeamTiltDirection(void);
  int FocusChangeLimits(void);
  int AbsoluteFocusLimits(void);
  int CorrectAstigmatism(void);
  int CorrectComa(void);
  int ZemlinTableau(void);
  int CBAstigComa(void);
  int FixAstigmatismByCTF(void);
  int ReportStigmatorNeeded(void);
  int ReportComaTiltNeeded(void);
  int ReportComaVsISmatrix(void);
  int Save(void);
  int ReadFile(void);
  int ReadOtherFile(void);
  int RetryReadOtherFile(void);
  int SaveToOtherFile(void);
  int OpenNewFile(void);
  int SetupWaffleMontage(void);
  int ReportNumMontagePieces(void);
  int EnterNameOpenFile(void);
  int ChooserForNewFile(void);
  int ReadTextFile(void);
  int OpenTextFile(void);
  int WriteLineToFile(void);
  int ReadLineToArray(void);
  int CloseTextFile(void);
  int FlushTextFile(void);
  int IsTextFileOpen(void);
  int UserSetDirectory(void);
  int SetNewFileType(void);
  int OpenOldFile(void);
  int CloseFile(void);
  int RemoveFile(void);
  int ReportCurrentFilename(void);
  int ReportFrameBaseName(void);
  int GetFileInWatchedDir(void);
  int AllowFileOverwrite(void);
  int SetDirectory(void);
  int ReportDirectory(void);
  int DoesFileExist(void);
  int MakeDateTimeDir(void);
  int SwitchToFile(void);
  int ReportFileNumber(void);
  int AddToAutodoc(void);
  int AddToFrameMdoc(void);
  int ReportFrameMdocOpen(void);
  int DeferWritingFrameMdoc(void);
  int OpenFrameMdoc(void);
  int CloseFrameMdoc(void);
  int AddToNextFrameStackMdoc(void);
  int AlignWholeTSOnly(void);
  int WriteComForTSAlign(void);
  int SaveLogOpenNew(void);
  int SaveLog(void);
  int DeferLogUpdates(void);
  int SaveCalibrations(void);
  int SetProperty(void);
  int ReportUserSetting(void);
  int SetUserSetting(void);
  int Copy(void);
  int Show(void);
  int ChangeFocus(void);
  int SetDefocus(void);
  int SetStandardFocus(void);
  int SetEucentricFocus(void);
  int CalEucentricFocus(void);
  int IncTargetDefocus(void);
  int SetTargetDefocus(void);
  int ReportAutofocusOffset(void);
  int SetAutofocusOffset(void);
  int SetObjFocus(void);
  int SetDiffractionFocus(void);
  int ResetDefocus(void);
  int SetMag(void);
  int SetMagIndex(void);
  int ChangeMag(void);
  int ChangeMagAndIntensity(void);
  int SetCamLenIndex(void);
  int SetSpotSize(void);
  int SetProbeMode(void);
  int Delay(void);
  int WaitForMidnight(void);
  int WaitForDose(void);
  int ScreenUp(void);
  int ScreenDown(void);
  int ReportScreen(void);
  int ReportScreenCurrent(void);
  int ImageShiftByPixels(void);
  int ImageShiftByUnits(void);
  int ImageShiftByMicrons(void);
  int ImageShiftByStageDiff(void);
  int ImageShiftToLastMultiHole(void);
  int ShiftImageForDrift(void);
  int ShiftCalSkipLensNorm(void);
  int CalibrateImageShift(void);
  int ReportFocusDrift(void);
  int TestSTEMshift(void);
  int QuickFlyback(void);
  int ReportAutoFocus(void);
  int ReportTargetDefocus(void);
  int SetBeamShift(void);
  int MoveBeamByMicrons(void);
  int MoveBeamByFieldFraction(void);
  int SetBeamTilt(void);
  int ReportBeamShift(void);
  int ReportBeamTilt(void);
  int SetImageShift(void);
  int AdjustBeamTiltforIS(void);
  int ReportImageShift(void);
  int SetObjectiveStigmator(void);
  int ReportXLensDeflector(void);
  int ReportSpecimenShift(void);
  int ReportObjectiveStigmator(void);
  int SuppressReports(void);
  int ErrorsToLog(void);
  int ReportAlignShift(void);
  int ReportAccumShift(void);
  int ResetAccumShift(void);
  int ReportAlignTrimming(void);
  int CameraToISMatrix(void);
  int ReportClock(void);
  int ResetClock(void);
  int ReportMinuteTime(void);
  int SetCustomTime(void);
  int ReportTickTime(void);
  int ElapsedTickTime(void);
  int ReportDateTime(void);
  int MoveStage(void);
  int RelaxStage(void);
  int BackgroundTilt(void);
  int ReportStageXYZ(void);
  int ReportTiltAngle(void);
  int ReportStageBusy(void);
  int ReportStageBAxis(void);
  int ReportMag(void);
  int ReportMagIndex(void);
  int ReportCameraLength(void);
  int ReportDefocus(void);
  int ReportFocus(void);
  int ReportPercentC2(void);
  int ReportCrossoverPercentC2(void);
  int ReportIlluminatedArea(void);
  int ReportImageDistanceOffset(void);
  int ReportAlpha(void);
  int ReportSpotSize(void);
  int ReportProbeMode(void);
  int ReportEnergyFilter(void);
  int ReportColumnMode(void);
  int ReportLens(void);
  int ReportCoil(void);
  int SetFreeLensControl(void);
  int SetLensWithFLC(void);
  int ReportLensFLCStatus(void);
  int SetJeolSTEMflags(void);
  int RemoveAperture(void);
  int PhasePlateToNextPos(void);
  int ReportPhasePlatePos(void);
  int ReportMeanCounts(void);
  int ReportFileZsize(void);
  int SubareaMean(void);
  int CircularSubareaMean(void);
  int ElectronStats(void);
  int CropImage(void);
  int ReduceImage(void);
  int FFT(void);
  int FilterImage(void);
  int CombineImages(void);
  int ScaleImage(void);
  int CtfFind(void);
  int ImageProperties(void);
  int ImageLowDoseSet(void);
  int MeasureBeamSize(void);
  int QuadrantMeans(void);
  int CheckForBadStripe(void);
  int CircleFromPoints(void);
  int FindPixelSize(void);
  int Echo(void);
  int EchoEval(void);
  int verbose(void);
  int ProgramTimeStamps(void);
  int IsVersionAtLeast(void);
  int Pause(void);
  int OKBox(void);
  int EnterOneNumber(void);
  int EnterString(void);
  int CompareNoCase(void);
  int StripEndingDigits(void);
  int MailSubject(void);
  int SendEmail(void);
  int ClearAlignment(void);
  int ResetImageShift(void);
  int ResetShiftIfAbove(void);
  int Eucentricity(void);
  int ReportLastAxisOffset(void);
  int SetTiltAxisOffset(void);
  int WalkUpTo(void);
  int ReverseTilt(void);
  int DriftWaitTask(void);
  int ConditionPhasePlate(void);
  int GetWaitTaskDrift(void);
  int BacklashAdjust(void);
  int CenterBeamFromImage(void);
  int AutoCenterBeam(void);
  int CookSpecimen(void);
  int SetIntensityByLastTilt(void);
  int SetDoseRate(void);
  int SetPercentC2(void);
  int SetIlluminatedArea(void);
  int SetImageDistanceOffset(void);
  int SetAlpha(void);
  int ReportJeolGIF(void);
  int SetJeolGIF(void);
  int NormalizeLenses(void);
  int NormalizeAllLenses(void);
  int ReportSlotStatus(void);
  int LoadCartridge(void);
  int RefrigerantLevel(void);
  int DewarsRemainingTime(void);
  int AreDewarsFilling(void);
  int ReportVacuumGauge(void);
  int ReportHighVoltage(void);
  int SetSlitWidth(void);
  int SetEnergyLoss(void);
  int SetSlitIn(void);
  int RefineZLP(void);
  int SelectCamera(void);
  int SetExposure(void);
  int SetBinning(void);
  int SetCameraArea(void);
  int SetCenteredSize(void);
  int SetExposureForMean(void);
  int SetContinuous(void);
  int SetProcessing(void);
  int SetFrameTime(void);
  int SetK2ReadMode(void);
  int SetDoseFracParams(void);
  int SetDECamFrameRate(void);
  int UseContinuousFrames(void);
  int StopContinuous(void);
  int ReportContinuous(void);
  int StartFrameWaitTimer(void);
  int WaitForNextFrame(void);
  int SetLiveSettleFraction(void);
  int SetSTEMDetectors(void);
  int RestoreCameraSetCmd(void);
  int KeepCameraSetChanges(void);
  int ReportK2FileParams(void);
  int SetK2FileParams(void);
  int ReportFrameAliParams(void);
  int SetFolderForFrames(void);
  int SkipFrameAliParamCheck(void);
  int ReportK3CDSmode(void);
  int SetK3CDSmode(void);
  int ReportCountScaling(void);
  int SetDivideBy2(void);
  int ReportNumFramesSaved(void);
  int CameraProperties(void);
  int ReportColumnOrGunValve(void);
  int SetColumnOrGunValve(void);
  int ReportFilamentCurrent(void);
  int SetFilamentCurrent(void);
  int IsPVPRunning(void);
  int SetBeamBlank(void);
  int MoveToNavItem(void);
  int RealignToNavItem(void);
  int RealignToMapDrawnOn(void);
  int GetRealignToItemError(void);
  int ReportNavItem(void);
  int ReportItemAcquire(void);
  int SetItemAcquire(void);
  int NavIndexWithLabel(void);
  int NavIndexItemDrawnOn(void);
  int NavItemFileToOpen(void);
  int SetNavItemUserValue(void);
  int SetMapAcquireState(void);
  int RestoreState(void);
  int ReportNumNavAcquire(void);
  int ReportNumTableItems(void);
  int SetNavRegistration(void);
  int SaveNavigator(void);
  int ReportIfNavOpen(void);
  int ReadNavFile(void);
  int CloseNavigator(void);
  int OpenNavigator(void);
  int ChangeItemRegistration(void);
  int SetItemTargetDefocus(void);
  int SetItemSeriesAngles(void);
  int SkipAcquiringNavItem(void);
  int SkipAcquiringGroup(void);
  int SkipMoveInNavAcquire(void);
  int SuspendNavRedraw(void);
  int StartNavAcquireAtEnd(void);
  int SuffixForExtraFile(void);
  int ItemForSuperCoord(void);
  int UpdateItemZ(void);
  int ReportGroupStatus(void);
  int ReportItemImageCoords(void);
  int NewMap(void);
  int MakeAnchorMap(void);
  int ShiftItemsByAlignment(void);
  int ShiftItemsByCurrentDiff(void);
  int ShiftItemsByMicrons(void);
  int AlignAndTransformItems(void);
  int ForceCenterRealign(void);
  int SkipPiecesOutsideItem(void);
  int FindHoles(void);
  int MakeNavPointsAtHoles(void);
  int ClearHoleFinder(void);
  int CombineHolesToMulti(void);
  int UndoHoleCombining(void);
  int SetHelperParams(void);
  int SetMontageParams(void);
  int ManualFilmExposure(void);
  int ExposeFilm(void);
  int GoToLowDoseArea(void);
  int SetLDContinuousUpdate(void);
  int SetLowDoseMode(void);
  int SetAxisPosition(void);
  int ReportLowDose(void);
  int CurrentSettingsToLDArea(void);
  int UpdateLowDoseParams(void);
  int RestoreLowDoseParamsCmd(void);
  int SetLDAddedBeamButton(void);
  int ShowMessageOnScope(void);
  int SetupScopeMessage(void);
  int UpdateHWDarkRef(void);
  int LongOperation(void);
  int NewDEserverDarkRef(void);
  int RunInShell(void);
  int NextProcessArgs(void);
  int CreateProcess(void);
  int ExternalToolArgPlace(void);
  int RunExternalTool(void);
  int SaveFocus(void);
  int RestoreFocus(void);
  int SaveBeamTilt(void);
  int RestoreBeamTilt(void);
  int SelectPiezo(void);
  int ReportPiezoXY(void);
  int ReportPiezoZ(void);
  int MovePiezoXY(void);
  int MovePiezoZ(void);
};

#endif  // MACROCOMMANDS_H
