// MacroCommands.cpp:    Subclass for executing the next command
//
// Copyright (C) 2003-2020 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <direct.h>
#include <process.h>
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include "MainFrm.h"
#include "ChildFrm.h"
#include ".\MacroProcessor.h"
#include "MacroEditer.h"
#include "MenuTargets.h"
#include "FocusManager.h"
#include "AutoTuning.h"
#include "ShiftManager.h"
#include "ShiftCalibrator.h"
#include "EMmontageController.h"
#include "MontageSetupDlg.h"
#include "LogWindow.h"
#include "EMscope.h"
#include "CameraController.h"
#include "GainRefMaker.h"
#include "CalibCameraTiming.h"
#include "TSController.h"
#include "EMbufferManager.h"
#include "ParameterIO.h"
#include "BeamAssessor.h"
#include "ProcessImage.h"
#include "ComplexTasks.h"
#include "MultiTSTasks.h"
#include "HoleFinderDlg.h"
#include "MultiHoleCombiner.h"
#include "AutoContouringDlg.h"
#include "ParticleTasks.h"
#include "FilterTasks.h"
#include "MacroControlDlg.h"
#include "MultiShotDlg.h"
#include "NavigatorDlg.h"
#include "NavHelper.h"
#include "NavRotAlignDlg.h"
#include "StateDlg.h"
#include "FalconHelper.h"
#include "ExternalTools.h"
#include "ScreenShotDialog.h"
#include "Mailer.h"
#include "PiezoAndPPControl.h"
#include "Utilities\XCorr.h"
#include "Utilities\KGetOne.h"
#include "Shared\b3dutil.h"
#include "Shared\iimage.h"
#include "Shared\autodoc.h"
#include "Shared\ctffind.h"
#include "Image\KStoreADOC.h"
#include "XFolderDialog\XFolderDialog.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

#define CMD_IS(a) (mCmdIndex == CME_##a)

#define ABORT_NOLINE(a) \
{ \
  mWinApp->AppendToLog((a), mLogErrAction);  \
  SEMMessageBox((a), MB_EXCLAME); \
  AbortMacro(); \
  return 1; \
}

#define ABORT_LINE(a) \
{ \
  mWinApp->AppendToLog((a) + mStrLine, mLogErrAction);  \
  SEMMessageBox((a) + mStrLine, MB_EXCLAME); \
  AbortMacro(); \
  return 1; \
}

#define ABORT_NONAV \
{ if (!mWinApp->mNavigator) \
  { \
    CString macStr = CString("The Navigator must be open to execute:\r\n") + mStrLine; \
    mWinApp->AppendToLog(macStr, mLogErrAction);  \
    SEMMessageBox(macStr, MB_EXCLAME); \
    AbortMacro(); \
    return 1; \
  } \
}

#define SUSPEND_NOLINE(a) \
{ \
  mCurrentIndex = mLastIndex; \
  CString macStr = CString("Script suspended ") + (a);  \
  mWinApp->AppendToLog(macStr, mLogErrAction);  \
  SEMMessageBox(macStr, MB_EXCLAME); \
  SuspendMacro(); \
  return 1; \
}

#define SUSPEND_LINE(a) \
{ \
  mCurrentIndex = mLastIndex; \
  CString macStr = CString("Script suspended ") + (a) + mStrLine;  \
  mWinApp->AppendToLog(macStr, mLogErrAction);  \
  SEMMessageBox(macStr, MB_EXCLAME); \
  SuspendMacro(); \
  return 1; \
}

#define MAC_SAME_NAME(nam, req, flg, cme) {#nam, req, flg | 8, &CMacCmd::nam},
#define MAC_DIFF_NAME(nam, req, flg, fnc, cme)  {#nam, req, flg | 8, &CMacCmd::fnc},
#define MAC_SAME_FUNC(nam, req, flg, fnc, cme)  {#nam, req, flg | 8, &CMacCmd::fnc},
#define MAC_SAME_NAME_NOARG(nam, req, flg, cme) {#nam, req, flg, &CMacCmd::nam},
#define MAC_DIFF_NAME_NOARG(nam, req, flg, fnc, cme)  {#nam, req, flg, &CMacCmd::fnc},
#define MAC_SAME_FUNC_NOARG(nam, req, flg, fnc, cme)  {#nam, req, flg, &CMacCmd::fnc},
#define MAC_SAME_NAME_ARG(a, b, c, d, e) MAC_SAME_NAME_NOARG(a, b, c, d)
#define MAC_DIFF_NAME_ARG(a, b, c, d, e, f) MAC_DIFF_NAME_NOARG(a, b, c, d, e)
#define MAC_SAME_FUNC_ARG(a, b, c, d, e, f) MAC_SAME_FUNC_NOARG(a, b, c, d, e)

static CmdItem cmdList[] = {
#include "MacroMasterList.h"
  {NULL, 0, 0}
};

// Be sure to add an entry for longHasTime when adding long operation
const char *CMacCmd::mLongKeys[MAX_LONG_OPERATIONS] =
  {"BU", "RE", "IN", "LO", "$=", "DA", "UN", "$=", "RS", "RT", "FF"};
int CMacCmd::mLongHasTime[MAX_LONG_OPERATIONS] = {1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1};

CMacCmd::CMacCmd() : CMacroProcessor()
{
  int i;
  unsigned int hash;
  SEMBuildTime(__DATE__, __TIME__);
  // 2/22/21: No more substitutions with the new macro-built table
  //  for (i = 0; i < 5; i++)
  //  cmdList[i + CME_VIEW].mixedCase = (LPCTSTR)mModeNames[i];
  CString cstr;
  mNumCommands = sizeof(cmdList) / sizeof(CmdItem);
  for (i = 0; i < mNumCommands - 1; i++) {
    cstr = cmdList[i].mixedCase;
    cstr.MakeUpper();
    cmdList[i].cmd = (LPCTSTR)cstr;
    if (cmdList[i].arithAllowed & 1)
      mArithAllowed.insert(cmdList[i].cmd);
    if (cmdList[i].arithAllowed & 2)
      mArithDenied.insert(cmdList[i].cmd);

    // Get a hash value from the upper case command string and map it to index, or to 0
    // if there is a collision
    hash = StringHashValue((LPCTSTR)cstr);
    if (!mCmdHashMap.count(hash))
      mCmdHashMap.insert(std::pair<unsigned int, int>(hash, i));
    else
      mCmdHashMap.insert(std::pair<unsigned int, int>(hash, 0));
  }
  mCmdList = &cmdList[0];
}

CMacCmd::~CMacCmd()
{
}

// When a task is done, if the macro flag is still set, call next command
void CMacCmd::TaskDone(int param)
{
  int arg, err, cmdInd;
  bool isEcho;
  mStrLine = "";
  if (DoingMacro()) {
    if (mRunningScrpLang) {
      if (mScrpLangData.threadDone) {

        // If thread finished, clear flag and abort/terminate for real
        mRunningScrpLang = false;
        if (mScrpLangData.threadDone > 0 || mScrpLangData.exitedFromWrapper)
          SEMMessageBox("Error running Python script; see log for information");
        if (mScrpLangData.threadDone < 0)
          mLastCompleted = true;
        AbortMacro();
        if (mLastCompleted && mStartNavAcqAtEnd)
          mWinApp->AddIdleTask(TASK_START_NAV_ACQ, 0, 0);

      } else if (mScrpLangData.waitingForCommand) {

        // If waiting for command, it has arrived -check if in range
        cmdInd = mScrpLangData.functionCode;
        if (cmdInd < 0 || cmdInd >= mNumCommands) {
          PrintfToLog("External script interpreter passed an out-of-bounds function code"
            " of %d (maximum is %d)", cmdInd, mNumCommands - 1);
          mScrpLangData.errorOccurred = 1;
          SetEvent(mScrpLangDoneEvent);
          SEMTrace('[', "signal out of bounds error");
          return;
        }
        InitForNextCommand();
        //SEMAcquireScriptLangMutex(1000, "run the operation");

        // Copy over the arguments and compose a line in case of error messages
        mCmdIndex = mScrpLangData.functionCode;
        isEcho = CMD_IS(ECHO) || CMD_IS(ECHOREPLACELINE);
        mStrLine = mCmdList[cmdInd].mixedCase;
        mStrItems[0] = mStrLine;
        mStrLine += isEcho ? " " : "(";
        mLastNonEmptyInd = mScrpLangData.lastNonEmptyInd;
        for (arg = 1; arg < MAX_SCRIPT_LANG_ARGS; arg++) {
          if (arg <= mLastNonEmptyInd) {

            // Copy actual arguments
            mStrItems[arg] = mScrpLangData.strItems[arg];
            mItemEmpty[arg] = mStrItems[arg].IsEmpty();
            mItemDbl[arg] = mScrpLangData.itemDbl[arg];
            if (arg > 1)
              mStrLine += ", ";
            mStrLine += mStrItems[arg];
          } else {

            // Clear out the rest
            mStrItems[arg] = "";
            mItemEmpty[arg] = true;
            mItemDbl[arg] = 0.;
          }
          mItemFlt[arg] = (float)mItemDbl[arg];
          mItemInt[arg] = (int)mItemDbl[arg];
        }
        if (!isEcho)
          mStrLine += ")";
        if (mVerbose)
          mWinApp->AppendToLog(mStrLine);
        mItem1upper = mStrItems[1];
        mItem1upper.MakeUpper();
        mScrpLangData.waitingForCommand = 0;
        if (mCmdIndex != CME_GETVARIABLE)
          ClearVariables(VARTYPE_REPORT);

        // Call the function in the command list
        err = (this->*cmdList[mCmdIndex].func)();
        if (err) {

          // Abort should have been called before an error return, but just in case, let's
          // do it now and let it signal done
          if (!mScrpLangData.errorOccurred)
            AbortMacro();
          return;
        }
        if (mCmdIndex != CME_EXIT)
          mScrpLangData.errorOccurred = 0;

        // Output the standard log report variable if set
        if (!mLogRpt.IsEmpty())
          mWinApp->AppendToLog(mLogRpt, mLogAction);

        // The action is taken or started: now set up an idle task unless looping is allowed
        // for this command and not too many loops have happened
        if (!((cmdList[mCmdIndex].arithAllowed & 4) || mLoopInOnIdle ||
          cmdList[mCmdIndex].cmd.find("REPORT") == 0) ||
          mNumCmdSinceAddIdle > mMaxCmdToLoopOnIdle) {
          mWinApp->AddIdleTask(NULL, TASK_MACRO_RUN, 0, 0);
          mNumCmdSinceAddIdle = 0;
        } else {

          // Or let it loop back in to NextCommand
          mNumCmdSinceAddIdle++;
          mLoopInOnIdle = true;
        }
        return;


      } else {

        // Command is done after looping back in: Set up to wait for next and to the part
        // NextCommand that checks and finalizes from last command
        mScrpLangData.waitingForCommand = 1;
        mScrpLangData.commandReady = 0;
        mScrpLangData.errorOccurred = 0;
        NextCommand(false);

        // That might have set error
        // The return values have already been set in the data structure, so ready to go
        SetEvent(mScrpLangDoneEvent);
        SEMTrace('[', "signal operations done after loop back in");
        mWinApp->AddIdleTask(NULL, TASK_MACRO_RUN, 0, 0);
      }

    } else {

      // Regular scripting here
      NextCommand(false);
    }
  } else
    SuspendMacro();
}

// Run the next command: start by checking the results of last command
int CMacCmd::NextCommand(bool startingOut)
{
  CString name, report;
  int index, index2, i, ix0, ix1, iy0, iy1, sizeX, sizeY, cartInd;
  float defocus, astig, angle, phase, fitFreq, ctfCCC;
  CArray<JeolCartridgeData, JeolCartridgeData> *loaderInfo = mScope->GetJeolLoaderInfo();
  CArray < ArrayRow, ArrayRow > *rowsFor2d;
  const char *strPtr;
  char *endPtr;
  ArrayRow arrRow;
  Variable *var;

  bool inComment = false;

  // Process an inventory of JEOL autoloader or a change in locations
  cartInd = mScope->GetChangedLoaderInfo();
  if (mStartedLongOp && cartInd) {
    arrRow.numElements = 6;
    if (cartInd < 0) {
      rowsFor2d = new CArray < ArrayRow, ArrayRow >;
      mWinApp->AppendToLog("Index ID     Location  Slot  Type Angle Name", mLogAction);
      for (index = 0; index < (int)loaderInfo->GetSize(); index++) {
        FormatCartridgeInfo(index, index2, ix0, ix1, iy0, iy1, name, report,arrRow.value);
        mWinApp->AppendToLog(report, mLogAction);
        rowsFor2d->Add(arrRow);
      }
      if (SetVariable("AUTOLOADERINFO", "0", VARTYPE_PERSIST, -1, false,
        &report, rowsFor2d)) {
        delete rowsFor2d;
        mWinApp->AppendToLog("Error setting autoloaderInfo variable with data:\n" + 
          report);
        AbortMacro();
        return 1;
      }
    } else {
      var = LookupVariable("AUTOLOADERINFO", index);
      if (var) {
        cartInd = mScope->GetLoadedCartridge();
        for (index = 0; index < 2; index++) {
          if (cartInd >= 0) {
            FormatCartridgeInfo(cartInd, index2, ix0, ix1, iy0, iy1, name, report,
              arrRow.value);
            var->rowsFor2d->SetAt(cartInd, arrRow);
          }
          cartInd = mScope->GetUnloadedCartridge();
        }
      }
    }
  }

  if (mRanGatanScript)
    SetReportedValues(mCamera->GetScriptReturnVal());

  // If ctfplotter was run, see if succeeded
  if (mRanCtfplotter) {
    if (mExtProcExitStatus) {
      SetReportedValues(1., mExtProcExitStatus);
    } else {
      index = mWinApp->mExternalTools->ReadCtfplotterResults(defocus, astig, angle, phase,
        index2, fitFreq, ctfCCC, name, report);
      if (index > 0) {
        mWinApp->AppendToLog("Error getting Ctfplotter results: " + report);
        AbortMacro();
        return 1;
      }
      if (index < 0)
        mWinApp->AppendToLog("WARNING: " + report);
      mWinApp->AppendToLog(name, mLogAction);
      SetReportedValues(defocus, astig, angle, phase, fitFreq, ctfCCC);
    }
  }

  InitForNextCommand();

  if (mMovedPiezo && mWinApp->mPiezoControl->GetLastMovementError()) {
    AbortMacro();
    return 1;
  }
  mMovedPiezo = false;

  // First test for termination conditions
  // Stopping conditions that do not allow resumption:
  if (mTestTiltAngle && ((mControl->limitTiltUp
    && mScope->GetTiltAngle() > mControl->tiltUpLimit) ||
    (mControl->limitTiltDown && mScope->GetTiltAngle() < mControl->tiltDownLimit))) {
    mWinApp->AppendToLog("Script stopped because tilt limit specified\r\n"
      " in Script Control dialog was reached", LOG_MESSAGE_IF_CLOSED);
    AbortMacro();
    return 1;
  }

  if (mControl->limitRuns && mNumRuns >= mControl->runLimit) {
    mWinApp->AppendToLog("Script stopped because script run limit specified\r\n"
      " in Script Control dialog was reached", LOG_MESSAGE_IF_CLOSED);
    AbortMacro();
    return 1;
  }

  // Pop control set that was to be restored after shot
  if (mRestoreConsetAfterShot) {
    mRestoreConsetAfterShot = false;
    i = mConsetNums.back();
    mConSets[i] = mConsetsSaved.back();
    mConsetNums.pop_back();
    mConsetsSaved.pop_back();
  }

  // Stopping conditions that are suspensions - but backing up is a bad idea
  if (mTestMontError && mWinApp->Montaging() && mControl->limitMontError &&
    mLastMontError > mControl->montErrorLimit) {
    mWinApp->AppendToLog("Script suspended because montage error specified\r\n"
      " in Script Control dialog exceeded limit", LOG_MESSAGE_IF_CLOSED);
    SuspendMacro();
    return 1;
  }

  index = mTestMontError ? 1 : 0;
  if (mControl->limitScaleMax && mImBufs[index].mImage && mImBufs[index].mImageScale &&
    mTestScale && mImBufs[index].mImageScale->GetMaxScale() < mControl->scaleMaxLimit) {
    mWinApp->AppendToLog("Script suspended because image intensity fell below limit "
      "specified\r\n in Script Control dialog ", LOG_MESSAGE_IF_CLOSED);
    SuspendMacro();
    return 1;
  }

  if (mControl->limitIS) {
    double ISX, ISY, specX, specY;
    ScaleMat cMat;
    mScope->GetLDCenteredShift(ISX, ISY);

    cMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
    specX = cMat.xpx * ISX + cMat.xpy * ISY;
    specY = cMat.ypx * ISX + cMat.ypy * ISY;
    if (cMat.xpx != 0.0 && sqrt(specX * specX + specY * specY) > mControl->ISlimit) {
      mWinApp->AppendToLog("Script suspended because image shift exceeded limit "
      "specified\r\n in Script Control dialog ", LOG_MESSAGE_IF_CLOSED);
      SuspendMacro();
      return 1;
    }
  }

  // Crop image now if that was needed
  if (mCropXafterShot > 0) {
    mImBufs->mImage->getSize(sizeX, sizeY);
    ix0 = (sizeX - mCropXafterShot) / 2;
    iy0 = (sizeY - mCropYafterShot) / 2;
    ix1 = ix0 + mCropXafterShot - 1;
    iy1 = iy0 + mCropYafterShot - 1;
    mCropXafterShot = -1;
    ix0 = mProcessImage->CropImage(mImBufs, iy0, ix0, iy1, ix1);
    if (ix0) {
      report.Format("Error # %d attempting to crop new image to match buffer", ix0);
      ABORT_NOLINE(report);
    }
    mImBufs->mCaptured = BUFFER_CROPPED;
  }

  if (mShowedScopeBox)
    SetReportedValues(1., mScope->GetMessageBoxReturnValue());
  mShowedScopeBox = false;

  // Checks of last command are done, so return for external scripting
  if (mRunningScrpLang)
    return 0;

  // Save the current index
  mLastIndex = mCurrentIndex;

  // Find the next real command
  mMacro = &mMacros[mCurrentMacro];
  mStrItems[0] = "";
  while (mStrItems[0].IsEmpty() && mCurrentIndex < mMacro->GetLength()) {

    GetNextLine(mMacro, mCurrentIndex, mStrLine);
    if (inComment) {
      if (mStrLine.Find("*/") >= 0)
        inComment = false;
      continue;
    }
    mStrCopy = mStrLine;

    // Parse the line
    if (mParamIO->ParseString(mStrCopy, mStrItems, MAX_MACRO_TOKENS,
      mParseQuotes) && !(mStrItems[1] == "@=" || mStrItems[1] == ":@="))
      ABORT_LINE("Too many items on line in script: \n\n");
    if (mStrItems[0].Find("/*") == 0) {
      mStrItems[0] = "";
      inComment = mStrLine.Find("*/") < 0;
      continue;
    }
    if (mVerbose > 0)
      mWinApp->AppendToLog("COMMAND: " + mStrLine, LOG_OPEN_IF_CLOSED);
    mStrItems[0].MakeUpper();
  }

  // Substitute variables in parsed items and check for control word substitution
  report = mStrItems[0];
  if (SubstituteVariables(mStrItems, MAX_MACRO_TOKENS, mStrLine)) {
    AbortMacro();
    return 1;
  }

  if (!mStrItems[0].IsEmpty()) {
    strPtr = (LPCTSTR)mStrItems[0];
    strtod(strPtr, &endPtr);
    if (endPtr == strPtr + strlen(strPtr))
      ABORT_LINE("A command cannot start with a number in line:\n\n");
  }

  mStrItems[0].MakeUpper();
  mItem1upper = mStrItems[1];
  mItem1upper.MakeUpper();
  if (report != mStrItems[0] && WordIsReserved(mStrItems[0]))
    ABORT_LINE("You cannot make a command by substituting a\n" 
      "variable value that is a control command on line: \n\n");

  mCmdIndex = LookupCommandIndex(mStrItems[0]);

  // Do arithmetic on selected commands
  if (mCmdIndex >= 0 && ArithmeticIsAllowed(mStrItems[0])) {
    if (SeparateParentheses(&mStrItems[1], MAX_MACRO_TOKENS - 1, mParseQuotes))
      ABORT_LINE("Too many items on line after separating out parentheses in line: \n\n");
    if (EvaluateExpression(&mStrItems[1], MAX_MACRO_TOKENS - 1, mStrLine, 0, index, 
      index2)) {
      AbortMacro();
      return 1;
    }
    for (i = index + 1; i <= index2; i++)
      mStrItems[i] = "";
    index = CheckLegalCommandAndArgNum(&mStrItems[0], mCmdIndex, mStrLine,mCurrentMacro);
    if (index) {
      if (index == 1)
        ABORT_LINE("There is no longer a legal command after evaluating arithmetic on "
          "this line:\n\n");
      AbortMacro();
      return 1;
    }
  }

  // Evaluate emptiness, ints and doubles
  for (i = 0; i < MAX_MACRO_TOKENS; i++) {
    mItemEmpty[i] = mStrItems[i].IsEmpty();
    mItemInt[i] = 0;
    mItemDbl[i] = 0.;
    if (!mItemEmpty[i]) {
      mItemInt[i] = atoi((LPCTSTR)mStrItems[i]);
      mItemDbl[i] = atof((LPCTSTR)mStrItems[i]);
      mLastNonEmptyInd = i;
    }
    mItemFlt[i] = (float)mItemDbl[i];
  }
  if (mItemEmpty[0]) {
    mCmdIndex = CME_SCRIPTEND;
  } else {
    if (mStrItems[0].GetAt(mStrItems[0].GetLength() - 1) == ':')
      mCmdIndex = CME_LABEL;
    else if (mStrItems[1] == "@=" || mStrItems[1] == ":@=")
      mCmdIndex = CME_SETSTRINGVAR;
    else if (mStrItems[1] == "=" || mStrItems[1] == ":=")
      mCmdIndex = CME_SETVARIABLE;
  }
  if (CMD_IS(ELSEIF) && mBlockLevel >= 0 && mLoopLimit[mBlockLevel] == LOOP_LIMIT_FOR_IF)
    mCmdIndex = CME_ZEROLOOPELSEIF;

  // See if we are supposed to stop at an ending place
  if (mStopAtEnd && (CMD_IS(REPEAT) || CMD_IS(ENDLOOP) || CMD_IS(DOMACRO) ||
    CMD_IS(DOSCRIPT))) {
    if (mLastIndex >= 0)
      mCurrentIndex = mLastIndex;
    SuspendMacro();   // Leave it resumable
    return 1;
  }

  mKeyBreak = CMD_IS(KEYBREAK) && ((mItemEmpty[1] && mKeyPressed == 'B') ||
    (mStrItems[1].GetLength() == 1 && mItem1upper.GetAt(0) == mKeyPressed));
  if (mKeyBreak)
    mCmdIndex = CME_DOKEYBREAK;

  if (!mStrItems[0].IsEmpty() && mCmdIndex < 0)
    ABORT_LINE("After variable substitution, " + mStrItems[0] +
      " is not a valid command in line:\n\n");

  // Call the function in the command list
  if ((this->*cmdList[mCmdIndex].func)())
    return 1;

  // Output the standard log report variable if set
  if (!mLogRpt.IsEmpty())
    mWinApp->AppendToLog(mLogRpt, mLogAction);

  // The action is taken or started: now set up an idle task unless looping is allowed
  // for this command and not too many loops have happened
  if (startingOut || !((cmdList[mCmdIndex].arithAllowed & 4) || mLoopInOnIdle ||
    cmdList[mCmdIndex].cmd.find("REPORT") == 0) ||
    mNumCmdSinceAddIdle > mMaxCmdToLoopOnIdle ||
    SEMTickInterval(mLastAddIdleTime) > 1000. * mMaxSecToLoopOnIdle) {
    mWinApp->AddIdleTask(NULL, TASK_MACRO_RUN, 0, 0);
    mNumCmdSinceAddIdle = 0;
    mLastAddIdleTime = GetTickCount();
  } else {

    // Or let it loop back in to NextCommand
    mNumCmdSinceAddIdle++;
    mLoopInOnIdle = true;
  }
  return 0;
}

// Set up pointers and other initializations for a command
void CMacCmd::InitForNextCommand()
{
  mReadOtherSleep = 2000;
  mCurrentCam = mWinApp->GetCurrentCamera();
  mFiltParam = mWinApp->GetFilterParams();
  mActiveList = mWinApp->GetActiveCameraList();
  mCamParams = mWinApp->GetCamParams() + mCurrentCam;
  mMontP = mWinApp->GetMontParam();
  mLdParam = mWinApp->GetLowDoseParams();
  mNavigator = mWinApp->mNavigator;
  mLogRpt = "";

  mSleepTime = 0.;
  mDoseTarget = 0.;
  mMovedStage = false;
  mMovedScreen = false;
  mExposedFilm = false;
  mStartedLongOp = false;
  mMovedAperture = false;
  mRanGatanScript = false;
  mRanExtProcess = false;
  mRanCtfplotter = false;
  mLoadingMap = false;
  mLoopInOnIdle = false;
}

// ScriptEnd, Exit, Return, EndFunction, Function
int CMacCmd::ScriptEnd(void)
{
  int index;

  if (!mCallLevel || CMD_IS(EXIT) || (CMD_IS(ENDFUNCTION) && mExitAtFuncEnd)) {
    if (CMD_IS(EXIT)) {
      if (mRunningScrpLang) {
        if (!mItemEmpty[1] && mItemInt[1]) {
          mScrpLangData.errorOccurred = SCRIPT_EXIT_NO_EXC;
          mScrpLangData.exitedFromWrapper = mItemInt[1] < 0;
        } else
          mScrpLangData.errorOccurred = SCRIPT_NORMAL_EXIT;
        if (!mItemEmpty[2])
          EnhancedExceptionToLog(mStrItems[2]);
      } else if (!mStrItems[1].IsEmpty()) {
        SubstituteLineStripItems(mStrLine, 1, mStrCopy);
        mWinApp->AppendToLog(mStrCopy);
      }
    } else if (mRunningScrpLang || mCalledFromScrpLang)
      mScrpLangData.errorOccurred = SCRIPT_NORMAL_EXIT;
    AbortMacro();
    mLastCompleted = !mExitAtFuncEnd;
    if (mLastCompleted && mStartNavAcqAtEnd)
      mWinApp->AddIdleTask(TASK_START_NAV_ACQ, 0, 0);
    return 1;
  }

  // For a return, pop any loops, clear index variables
  LeaveCallLevel(CMD_IS(RETURN));
  if (mRunningScrpLang && mCalledFromSEMmacro) {
    mRunningScrpLang = false;
    mCalledFromSEMmacro = false;
    mScrpLangData.errorOccurred = SCRIPT_NORMAL_EXIT;
    SetEvent(mScrpLangDoneEvent);
    SEMTrace('[', "signal normal exit on return");
  }

  // Put return values in repVals
  if (CMD_IS(RETURN) && !mItemEmpty[1]) {
    ClearVariables(VARTYPE_REPORT);
    for (index = 1; index <= 6; index++) {
      if (mItemEmpty[index])
        break;
      SetOneReportedValue(mStrItems[index], index);
    }
  }
  return 0;
}

// Repeat
int CMacCmd::Repeat(void)
{
  mCurrentIndex = 0;
  mLastIndex = -1;
  mNumRuns++;
  return 0;
}

// Label, Require, KeyBreak, MacroName, ScriptName, LongName, ReadOnlyUnlessAdmin
int CMacCmd::NoOperation(void)
{
  return 0;
}

// KeyBreak for external scripting
int CMacCmd::KeyBreak(void)
{
  int index;

  if (mRunningScrpLang) {
    index = ((mItemEmpty[1] && mKeyPressed == 'B') ||
      (mStrItems[1].GetLength() == 1 && mItem1upper.GetAt(0) == mKeyPressed)) ? 1 : 0;
    if (index)
      mKeyPressed = 0;
    SetOneReportedValue(index, 1);
  }
  mKeyPressed = 0;
  return 0;
}

// EndLoop
int CMacCmd::EndLoop(void)
{

  // First see if we are actually doing a loop: if not, error
  if (mBlockDepths[mCallLevel] < 0 || mLoopLimit[mBlockLevel] < LOOP_LIMIT_FOR_IF ||
    mLoopLimit[mBlockLevel] == LOOP_LIMIT_FOR_TRY) {
    ABORT_NOLINE("The script contains an ENDLOOP without a LOOP or DOLOOP statement");
  }

  // If count is not past limit, go back to start;
  // otherwise clear index variable and decrease level indexes
  mLoopCount[mBlockLevel] += mLoopIncrement[mBlockLevel];
  if ((mLoopIncrement[mBlockLevel] < 0 ? - 1 : 1) *
    (mLoopCount[mBlockLevel] - mLoopLimit[mBlockLevel]) <= 0)
    mCurrentIndex = mLoopStart[mBlockLevel];
  else {
    ClearVariables(VARTYPE_INDEX, mCallLevel, mBlockLevel);
    mBlockLevel--;
    mBlockDepths[mCallLevel]--;
  }
  mLastIndex = -1;
  return 0;
}

// Loop, DoLoop
int CMacCmd::Loop(void)
{
  CString report;
  int index;

  // Doing a loop: get the count, make sure it is legal, and save the current
  // index as the starting place to go back to
  if (mBlockLevel >= MAX_LOOP_DEPTH)
    ABORT_LINE("Nesting of loops, IF blocks, and script or function calls is too deep"
    " at line: \n\n:");
  mBlockLevel++;
  mBlockDepths[mCallLevel]++;
  mLoopStart[mBlockLevel] = mCurrentIndex;
  mLoopIncrement[mBlockLevel] = 1;
  if (CMD_IS(LOOP)) {
    mLoopLimit[mBlockLevel] = B3DMAX(0, B3DNINT(mItemDbl[1]));
    mLoopCount[mBlockLevel] = 1;
    index = 2;
  } else {
    mLoopCount[mBlockLevel] = mItemInt[2];
    mLoopLimit[mBlockLevel] = mItemInt[3];
    if (!mItemEmpty[4]) {
      mLoopIncrement[mBlockLevel] = mItemInt[4];
      if (!mItemInt[4])
        ABORT_LINE("The loop increment is 0 at line:\n\n");
    }
    index = 1;
  }
  mLastIndex = -1;
  if ((mLoopIncrement[mBlockLevel] < 0 ? - 1 : 1) *
    (mLoopCount[mBlockLevel] - mLoopLimit[mBlockLevel]) > 0) {
    if (SkipToBlockEnd(SKIPTO_ENDLOOP, mStrLine)) {
      AbortMacro();
      return 1;
    }
  } else if (!mItemEmpty[index]) {
    if (SetVariable(mStrItems[index], 1.0, VARTYPE_INDEX + VARTYPE_ADD_FOR_NUM,
      mBlockLevel, true, &report))
      ABORT_LINE(report + " in script line: \n\n");
  }
  return 0;
}

// If, ZeroLoopElseIf
int CMacCmd::If(void)
{
  BOOL truth;

  // IF statement: evaluate it, go up one block level with 0 limit
  if (EvaluateIfStatement(&mStrItems[1], MAX_MACRO_TOKENS - 1, mStrLine, truth)) {
    AbortMacro();
    return 1;
  }
  if (CMD_IS(IF)) {                                       // If
    mBlockLevel++;
    if (mBlockLevel >= MAX_LOOP_DEPTH)
      ABORT_LINE("Nesting of loops, IF blocks, and script or function calls is too deep"
      " at line: \n\n:");
    mBlockDepths[mCallLevel]++;
    mLoopLimit[mBlockLevel] = LOOP_LIMIT_FOR_IF;
  }

  // If not true, skip forward; if it is true, mark if as satisfied with a -1
  if (!truth) {
    if (SkipToBlockEnd(SKIPTO_ELSE_ENDIF, mStrLine) ){
      AbortMacro();
      return 1;
    }
    mLastIndex = -1;
  } else
    mLoopLimit[mBlockLevel] = LOOP_LIMIT_FOR_IF - 1;
  return 0;
}

// Endif
int CMacCmd::Endif(void)
{

  // Trust the initial check
  mBlockLevel--;
  mBlockDepths[mCallLevel]--;
  mLastIndex = -1;
  return 0;
}

// Else, ElseIf
int CMacCmd::Else(void)
{
  if (SkipToBlockEnd(SKIPTO_ENDIF, mStrLine)) {
    AbortMacro();
    return 1;
  }
  mLastIndex = -1;
  return 0;
}

// Break, Continue, DoKeyBreak
int CMacCmd::Break(void)
{
  int index, index2;

  if (SkipToBlockEnd(SKIPTO_ENDLOOP, mStrLine, &index, &index2)) {
    AbortMacro();
    return 1;
  }
  mLastIndex = -1;

  // Pop any IFs and TRYs on the loop stack
  while (mBlockLevel >= 0 && mLoopLimit[mBlockLevel] <= LOOP_LIMIT_FOR_TRY) {
    mBlockLevel--;
    mBlockDepths[mCallLevel]--;
  }
  mTryCatchLevel += index2;
  if (mBlockLevel >= 0 && (CMD_IS(BREAK) || mKeyBreak))
    mLoopCount[mBlockLevel] = mLoopLimit[mBlockLevel];
  if (mKeyBreak) {
    PrintfToLog("Broke out of loop after %c key pressed", (char)mKeyPressed);
    mKeyPressed = 0;
  }
  return 0;
}

// Try
int CMacCmd::Try(void)
{
  mBlockLevel++;
  if (mBlockLevel >= MAX_LOOP_DEPTH)
    ABORT_LINE("Nesting of loops, IF or TRY blocks, and script or function calls is too"
      " deep at line: \n\n:");
  mBlockDepths[mCallLevel]++;
  mLoopLimit[mBlockLevel] = LOOP_LIMIT_FOR_TRY;
  mTryCatchLevel++;
  return 0;
}

// Catch
int CMacCmd::Catch(void)
{
  if (SkipToBlockEnd(SKIPTO_ENDTRY, mStrLine)) {
    AbortMacro();
    return 1;
  }
  mTryCatchLevel--;
  mLastIndex = -1;
  return 0;
}

// EndTry
int CMacCmd::EndTry(void)
{
  mBlockLevel--;
  mBlockDepths[mCallLevel]--;
  mLastIndex = -1;
  return 0;
}

// Throw
int CMacCmd::Throw(void)
{
  mStrCopy = "";
  if (!mStrItems[1].IsEmpty())
    SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  if (TestTryLevelAndSkip(&mStrCopy))
    return 1;
  ABORT_NOLINE("Stopping because a THROW statement was encountered outside of a TRY "
    "block");
  return 0;
}

// SkipTo
int CMacCmd::SkipTo(void)
{
  int index, index2, i;

  if (SkipToLabel(mItem1upper, mStrLine, index, index2)) {
    AbortMacro();
    return 1;
  }

  // Pop proper number of ifs and loops from stack
  for (i = 0; i < index && mBlockLevel >= 0; i++) {
    ClearVariables(VARTYPE_INDEX, mCallLevel, mBlockLevel);
    mBlockLevel--;
    mBlockDepths[mCallLevel]--;
  }
  mTryCatchLevel += index2;
  mLastIndex = -1;
  return 0;
}

// DoMacro, DoScript, CallMacro, CallScript, Call, CallFunction, CallStringArray
// RunScriptAfterNavAcquire
int CMacCmd::DoMacro(void)
{
  CString report;
  BOOL truth;
  int index, index2, ix0, ix1;
  MacroFunction *func;
  bool fromScrpLang = mRunningScrpLang;

  // Skip any of these operations if we are in a termination function
  if (!mExitAtFuncEnd) {


    // Calling a macro or function: rely on initial error checks, get the number
    index2 = 0;
    func = NULL;
    if (CMD_IS(CALLFUNCTION)) {
      func = FindCalledFunction(
        B3DCHOICE(mRunningScrpLang, "CallFunction " + mStrItems[1], mStrLine), false,
        index, ix0);
      if (!func) {
        AbortMacro();
        return 1;
      }
      index2 = func->startIndex;
      if (func->wasCalled)
        ABORT_LINE("Trying to call a function already being called in line: \n\n");
      func->wasCalled = true;
    } else if (CMD_IS(CALL)) {
      index = FindCalledMacro(mStrLine, false, mRunningScrpLang ? mStrItems[1] : "");
      if (index < 0) {
        AbortMacro();
        return 1;
      }
    } else if (CMD_IS(CALLSTRINGARRAY)) {
      index = MakeNewTempMacro(mStrItems[1], mStrItems[2], true, mStrLine);
      if (!index)
        return 1;
      ix0 = 0;
      if (!CheckForScriptLanguage(index, true) && CheckBlockNesting(index, -1, ix0)) {
        AbortMacro();
        return 1;
      }

    } else {
      if ((CMD_IS(CALLMACRO) || CMD_IS(CALLSCRIPT) || CMD_IS(RUNSCRIPTAFTERNAVACQUIRE)) 
        && mItemInt[1] == -1) {
        if (mScriptNumFound < 0)
          ABORT_LINE("No script number was previously found with FindScriptByName for "
            "line:\n\n");
        index = mScriptNumFound;
      } else {
        index = mItemInt[1] - 1;
        if (index < 0 || index >= MAX_MACROS)
          ABORT_LINE("Illegal script number in line:\n\n")
      }
      if (CMD_IS(RUNSCRIPTAFTERNAVACQUIRE)) {
        ABORT_NONAV;
        mNavigator->SetRunScriptAfterNextAcq(index);
        return 0;
      }
    }

    // Save the current index at this level and move up a level
    // If doing a function with string arg, substitute in line for that first so the
    // pre-existing level applies
    if (!(CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT))) {
      if (mCallLevel >= MAX_CALL_DEPTH)
        ABORT_LINE("Trying to call too many levels of scripts/functions in line: \n\n");
      if (func && func->ifStringArg)
        SubstituteVariables(&mStrLine, 1, mStrLine);
      if (mRunningScrpLang && mCalledFromSEMmacro) {
        ABORT_LINE("You cannot run any other script from a Python script called from a "
          "regular script in line:\n\n");
      }
      if ((mRunningScrpLang || mCalledFromScrpLang) &&
        CheckForScriptLanguage(index, true)) {
        ABORT_LINE("You cannot run another Python script from a Python script in "
          "line:\n\n");
      }
      if (mRunningScrpLang) {

        // Switching from Python to regular
        ix0 = 0;
        if (!CMD_IS(CALLSTRINGARRAY)) {
          if (mMacroEditer[index])
            mMacroEditer[index]->TransferMacro(true);
          PrepareForMacroChecking(index);
          if (CheckBlockNesting(index, -1, ix0)) {
            AbortMacro();
            return 1;
          }

          // That check rebuilt the function arrays, so need to find it again
          if (func) {
            func = FindCalledFunction("CallFunction " + mStrItems[1], false, index,
              ix0);
            if (!func) {
              AbortMacro();
              return 1;
            }
            ix0 = 3;
          }
          mCallLevel = 0;
        }
        mCalledFromScrpLang = true;
        mRunningScrpLang = false;
      } else if (!func) {

        // Running regular: see if what is being called is Python and switch if so
        ix1 = CheckForScriptLanguage(index, false, 
          (CMD_IS(CALLMACRO) || CMD_IS(CALLSCRIPT)) ? 2 : 0);
        if (ix1 > 0) {
          AbortMacro();
          return 1;
        }
        if (ix1 < 0) {
          mRunningScrpLang = true;
          mCalledFromSEMmacro = true;
          StartRunningScrpLang();
        }
      }
      mCallIndex[mCallLevel++] = mCurrentIndex;
      mCallMacro[mCallLevel] = index;
      mBlockDepths[mCallLevel] = -1;
      mCallFunction[mCallLevel] = func;

    } else {
      mNumRuns++;
    }

    // Doing another macro (or this one over), just reset macro # and index
    mCurrentMacro = index;
    mCurrentIndex = index2;
    mLastIndex = -1;
    if (func)
      SetVariable("FUNCTIONNAME", func->name, VARTYPE_LOCAL, -1, false);

    // Create argument variables now that level is raised
    if (func && (func->numNumericArgs || func->ifStringArg)) {
      truth = false;
      for (index = 0; index < func->numNumericArgs; index++) {
        report = func->argNames[index];
        if (SetVariable(func->argNames[index], mItemEmpty[index + ix0] ?
          0. : mItemDbl[index + ix0], VARTYPE_LOCAL + VARTYPE_ADD_FOR_NUM, -1, false,
          &report)) {
            truth = true;
            break;
        }
      }
      if (!truth && func->ifStringArg) {
        if (fromScrpLang)
          mStrCopy = mStrItems[2];
        else
          JustStripItems(mStrLine, index + ix0, mStrCopy);
        truth = SetVariable(func->argNames[index], mStrCopy, VARTYPE_LOCAL,
          - 1, false, &report);
      }
      if (truth)
        ABORT_LINE("Failed to set argument variables in function call:\r\n" + report +
        " in line:\n\n")
      for (index = 0; index < func->numNumericArgs + (func->ifStringArg ? 1 : 0);
        index++)
          if (mItemEmpty[index + ix0])
            break;
      SetVariable("NUMCALLARGS", index, VARTYPE_LOCAL + VARTYPE_ADD_FOR_NUM, -1, false);
   }
  }
  return 0;
}

// PythonScript
int CMacCmd::PythonScript(void)
{
  int startLine, err, lastInd = 0, currentInd = mCurrentIndex;
  if (mCalledFromScrpLang)
    ABORT_NOLINE("You cannot run the Python after a PythonScript line\r\n"
      "  from regular scripting called from a Python script");
  if (!IsEmbeddedPythonOK(mCurrentMacro, mCurrentIndex, lastInd, currentInd))
    ABORT_NOLINE("PythonScript line with no matching EndPythonScript line");

  startLine = CountLinesToCurIndex(mCurrentMacro, mCurrentIndex);

  err = CheckForScriptLanguage(mCurrentMacro, false, 1, mCurrentIndex, lastInd, 
    startLine);
  if (err > 0) {
    AbortMacro();
  }
  if (!err)
    ABORT_NOLINE("Did not find #!Python after a PythonScript line");

  // Set flags, raise the call level, set up to come back after the End line, start it
  mRunningScrpLang = true;
  mCalledFromSEMmacro = true;
  mCallIndex[mCallLevel++] = currentInd;
  mCallMacro[mCallLevel] = mCurrentMacro;
  mBlockDepths[mCallLevel] = -1;
  mCallFunction[mCallLevel] = NULL;
  mLastIndex = -1;
  StartRunningScrpLang();
  return 0;
}

// FindScriptByName
int CMacCmd::FindScriptByName(void)
{
  mScriptNumFound = FindCalledMacro(mStrLine, false);
  if (mScriptNumFound < 0) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// ParseQuotedStrings
int CMacCmd::ParseQuotedStrings(void)
{
  mParseQuotes = mItemEmpty[1] || mItemInt[1] != 0;
  return 0;
}

// StringArrayToScript
int CMacCmd::StringArrayToScript(void)
{
  int index;

  index = MakeNewTempMacro(mStrItems[1], mStrItems[2], false, mStrLine);
  if (!index)
    return 1;
  return 0;
}

// OnStopCallFunc
int CMacCmd::OnStopCallFunc(void)
{
  int ix0;
  MacroFunction *func;

  if (!mExitAtFuncEnd) {
    func = FindCalledFunction(mStrLine, false, mOnStopMacroIndex, ix0);
    if (!func) {
      AbortMacro();
      return 1;
    }
    mOnStopLineIndex = func->startIndex;
    if (func->numNumericArgs || func->ifStringArg)
      ABORT_LINE("The function to call takes arguments, which is not allowed here:"
      "\n\n");
  }
  return 0;
}

// NoMessageBoxOnError
int CMacCmd::NoMessageBoxOnError(void)
{
  mNoMessageBoxOnError = true;
  if (!mItemEmpty[1])
    mNoMessageBoxOnError = mItemInt[1] != 0;
  return 0;
}

// Test
int CMacCmd::Test(void)
{
  if (mItemEmpty[2])
    mLastTestResult = mItemDbl[1] != 0.;
  else
    EvaluateIfStatement(&mStrItems[1], MAX_MACRO_TOKENS - 1, mStrLine, mLastTestResult);
  SetReportedValues(mLastTestResult ? 1. : 0.);
  SetVariable("TESTRESULT", mLastTestResult ? 1. : 0.,
    VARTYPE_REGULAR + VARTYPE_ADD_FOR_NUM, -1, false);
  return 0;
}

// SetVariable, AppendToArray
int CMacCmd::SetVariableCmd(void)
{
  CString report;
  BOOL truth;
  int index, index2, ix0, ix1;
  Variable *var;
  CString *valPtr;
  int *numElemPtr;

  if (mRunningScrpLang) {
    index = CMD_IS(SETPERSISTENTVAR) ? VARTYPE_PERSIST : VARTYPE_REGULAR;
    if (CMD_IS(SETFLOATVARIABLE)) {
      index = (mItemInt[3] ? VARTYPE_PERSIST : VARTYPE_REGULAR) + VARTYPE_ADD_FOR_NUM;
    }
    if (SetVariable(mItem1upper, mStrItems[2], index, -1, false, &report))
      ABORT_LINE(report + " in script line: \n\n");
    return 0;
   }

  // Do assignment to variable before any non-reserved commands
  index2 = 2;
  ix0 = CheckForArrayAssignment(mStrItems, index2);
  if (SeparateParentheses(&mStrItems[index2], MAX_MACRO_TOKENS - index2, mParseQuotes))
    ABORT_LINE("Too many items on line after separating out parentheses in line: \n\n");
  if (EvaluateExpression(&mStrItems[index2], MAX_MACRO_TOKENS - index2, mStrLine, ix0,
    index, ix1)) {
      AbortMacro();
      return 1;
  }

  // Concatenate array elements separated by \n
  if (ix0 > 0) {
    for (ix1 = index2 + 1; ix1 < MAX_MACRO_TOKENS; ix1++) {
      if (mStrItems[ix1].IsEmpty())
        break;
      mStrItems[index2] += "\n" + mStrItems[ix1];
    }
  }

  // Set the variable
  if (CMD_IS(SETVARIABLE)) {
    index = mStrItems[1] == "=" ? VARTYPE_REGULAR : VARTYPE_PERSIST;
    if (SetVariable(mStrItems[0], mStrItems[index2], index, -1, false, &report))
      ABORT_LINE(report + " in script line: \n\n");

  } else {

    // Append to array: Split off an index to look it up
    ArrayRow newRow;

    var = GetVariableValuePointers(mItem1upper, &valPtr, &numElemPtr, "append to",
      report);
    if (!var)
      ABORT_LINE(report + " for line:\n\n");
    truth = valPtr == NULL;
    if (truth) {
      valPtr = &newRow.value;
      numElemPtr = &newRow.numElements;
    }

    // Add the string to the value and compute the number of elements
    if (!valPtr->IsEmpty())
      *valPtr += "\n";
    *valPtr += mStrItems[index2];
    *numElemPtr = 1;
    ix0 = -1;
    while ((ix0 = valPtr->Find('\n', ix0 + 1)) >= 0)
      *numElemPtr += 1;

    // Both 1D array and existing row of 2D array should be done: add new row to 2D
    if (truth)
      var->rowsFor2d->Add(newRow);
  }
  return 0;
}

// GetVariable
int CMacCmd::GetVariable(void)
{
  CString report;
  double delISX;
  int ix0;
  Variable *var;

  var = LookupVariable(mStrItems[1], ix0);
  if (!var)
    ABORT_LINE("The variable " + mStrItems[1] + " is not defined in line:\n\n");
  report = var->value;
  ClearVariables(VARTYPE_REPORT);
  if (var->isNumeric) {
    delISX = atof(report);
    SetOneReportedValue(delISX, 1);
  } else {
    SetOneReportedValue(report, 1);
  }
  return 0;
}

// TruncateArray
int CMacCmd::TruncateArray(void)
{
  CString report;
  int index, ix0, ix1;
  Variable *var;
  CString *valPtr;
  int *numElemPtr;

  index = mItemInt[2];
  if (index < 1)
    ABORT_LINE("The number to truncate to must be at least 1 for line:\n\n");
  var = GetVariableValuePointers(mItem1upper, &valPtr, &numElemPtr, "truncate",
    report);
  if (!var)
    ABORT_LINE(report + " for line:\n\n");
  if (!valPtr) {
    if (var->rowsFor2d->GetSize() > index)
      var->rowsFor2d->SetSize(index);
  } else if (*numElemPtr > index) {
    FindValueAtIndex(*valPtr, index, ix0, ix1);
    *valPtr = valPtr->Left(ix1);
    *numElemPtr = index;
  }
  return 0;
}

// ArrayStatistics
int CMacCmd::ArrayStatistics(void)
{
  CString report;
  int index, index2;
  float bmin, bmax, bmean, bSD, cpe;
  float *fvalues = FloatArrayFromVariable(mItem1upper, index2, report);
  if (!fvalues)
    ABORT_LINE(report + " in line\n\n");
  bmin = 1.e37f;
  bmax = -1.e37f;
  for (index = 0; index < index2; index++) {
    ACCUM_MIN(bmin, fvalues[index]);
    ACCUM_MAX(bmax, fvalues[index]);
  }
  avgSD(fvalues, index2, &bmean, &bSD, &cpe);
  rsFastMedianInPlace(fvalues, index2, &cpe);
  mLogRpt.Format("n= %d  min= %.6g  max = %.6g  mean= %.6g  sd= %.6g  median= %.6g",
    index2, bmin, bmax, bmean, bSD, cpe);
  SetRepValsAndVars(2, index2, bmin, bmax, bmean, bSD, cpe);

  delete[] fvalues;
  return 0;
}

// LinearFitToVars
int CMacCmd::LinearFitToVars(void)
{
  CString report;
  BOOL truth;
  int index, index2, ix0;
  float bmean, bSD, cpe;
  float *xx1, *xx2, *xx3 = NULL;
  truth = !mItemEmpty[3];
  xx1 = FloatArrayFromVariable(mItem1upper, index, report);
  if (!xx1)
    ABORT_LINE(report + " in line\n\n");
  xx2 = FloatArrayFromVariable(mStrItems[2], index2, report);
  if (!xx2) {
    delete[] xx1;
    ABORT_LINE(report + " in line\n\n");
  }
  if (truth) {
    xx3 = FloatArrayFromVariable(mStrItems[3], ix0, report);
    if (!xx3) {
      delete[] xx1, xx2;
      ABORT_LINE(report + " in line\n\n");
    }
  }
  if (index != index2 || (truth && ix0 != index)) {
    delete[] xx1, xx2, xx3;
    ABORT_LINE("Variables do not have the same number of elements in line:\n\n");
  }
  if (index < 2 || (truth && index < 3)) {
    delete[] xx1, xx2, xx3;
    ABORT_LINE("There are not enough array values for a linear fit in line:\n\n");
  }
  if (truth) {
    lsFit2(xx1, xx2, xx3, index, &bmean, &bSD, &cpe);
    mLogRpt.Format("n= %d  a1= %f  a2= %f  c= %f", index, bmean, bSD, cpe);
  } else {
    lsFit(xx1, xx2, index, &bmean, &bSD, &cpe);
    mLogRpt.Format("n= %d  slope= %f  intercept= %f  ro= %.4f", index, bmean, bSD,
      cpe);
  }
  SetReportedValues(index, bmean, bSD, cpe);
  delete[] xx1, xx2, xx3;
  return 0;
}

// SetStringVar
int CMacCmd::SetStringVar(void)
{
  CString report;
  int index;

  SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  mStrCopy.Replace("\\n", "\n");
  index = mStrItems[1] == "@=" ? VARTYPE_REGULAR : VARTYPE_PERSIST;
  if (SetVariable(mStrItems[0], mStrCopy, index, -1, false, &report))
    ABORT_LINE(report + " in script line: \n\n");
  return 0;
}

// ListVars
int CMacCmd::ListVars(void)
{
  ListVariables();
  return 0;
}

// ListPersistentVars
int CMacCmd::ListPersistentVars(void)
{
  ListVariables(VARTYPE_PERSIST);
  return 0;
}

// ClearPersistentVars
int CMacCmd::ClearPersistentVars(void)
{
  ClearVariables(VARTYPE_PERSIST);
  return 0;
}

// MakeVarPersistent
int CMacCmd::MakeVarPersistent(void)
{
  int index, ix0;
  Variable *var;

  var = LookupVariable(mStrItems[1], ix0);
  if (!var)
    ABORT_LINE("The variable " + mStrItems[1] + " is not defined in line:\n\n");
  index = 1;
  if (!mItemEmpty[2] && !mItemInt[2])
    index = 0;
  if ((index && var->type != VARTYPE_REGULAR) ||
    (!index && var->type != VARTYPE_PERSIST))
    ABORT_LINE("The variable " + mStrItems[1] + " must be " +
      CString(index ? "regular" : "persistent") + " to change its type in line:\n\n");
  var->type = index ? VARTYPE_PERSIST : VARTYPE_REGULAR;
  return 0;
}

// IsVariableDefined
int CMacCmd::IsVariableDefined(void)
{
  int index, index2;

  index = B3DCHOICE(LookupVariable(mItem1upper, index2) != NULL, 1, 0);
  SubstituteVariables(&mStrLine, 1, mStrLine);
  if (!mRunningScrpLang)
    mParamIO->ParseString(mStrLine, mStrItems, MAX_MACRO_TOKENS, mParseQuotes);
  mLogRpt.Format("Variable %s %s defined", (LPCTSTR)mStrItems[1],
    B3DCHOICE(index, "IS", "is NOT"));
  SetRepValsAndVars(2, index);
  return 0;
}

// NewArray, New2DArray
int CMacCmd::NewArrayCmd(void)
{
  CString report;
  BOOL truth;
  int index, index2, ix0, ix1, iy0;
  CArray < ArrayRow, ArrayRow > *rowsFor2d = NULL;
  ArrayRow arrRow;

  // Process local vesus persistent property
  ix0 = VARTYPE_REGULAR;
  if (mItemInt[2] < 0) {
    ix0 = VARTYPE_LOCAL;
    if (LocalVarAlreadyDefined(mStrItems[1], mStrLine) > 0)
      return 1;
  }
  if (mItemInt[2] > 0)
    ix0 = VARTYPE_PERSIST;

  // Get number of elements: 3 for 1D and 4 for 2D
  truth = CMD_IS(NEW2DARRAY);
  index = !mItemEmpty[3] ? mItemInt[3] : 0;
  if (truth) {
    index2 = !mItemEmpty[3] ? mItemInt[3] : 0;
    index = !mItemEmpty[4] ? mItemInt[4] : 0;
    if (index > 0 && !index2)
      ABORT_LINE("The number of elements per row must be 0 because no rows are created"
      " in:\n\n");
  }
  if (index < 0 || (truth && index2 < 0))
    ABORT_LINE("The number of elements to create must be positive in:\n\n");
  mStrCopy = "";
  for (ix1 = 0; ix1 < index; ix1++)
    mStrCopy += ix1 ? "\n0" : "0";

  // Create the 2D array rows and add string to the rows
  if (truth) {
    rowsFor2d = new CArray < ArrayRow, ArrayRow > ;
    arrRow.value = mStrCopy;
    arrRow.numElements = index;
    for (iy0 = 0; iy0 < index2; iy0++)
      rowsFor2d->Add(arrRow);
  }
  if (SetVariable(mItem1upper, truth ? "0" : mStrCopy, ix0, -1, false, &report,
    rowsFor2d)) {
    delete rowsFor2d;
    ABORT_LINE(report + " in script line: \n\n");
  }
  return 0;
}

// ImageMetadataToVar, ReportMetadataValues, ReportMetadataString
int CMacCmd::ImageMetadataToVar(void)
{
  EMimageBuffer *imBuf;
  int index, adocInd, sectInd;
  char *sectName = "ImageMetadata";
  char *valString;
  char buffer[20001];
  CString report, bufStr;
  buffer[20000] = 0x00;
  if (ConvertBufferLetter(mStrItems[1], 0, true, index, report, true))
    ABORT_LINE(report);
  imBuf = ImBufForIndex(index);
  if (!AdocAcquireMutex())
    ABORT_LINE("Error getting mutex to autodoc structures for line:\n\n");
  adocInd = AdocNew();
  if (adocInd < 0)
    report = "Error getting new autodoc structure";
  else {
    _snprintf(buffer, 20000, "%d", index < 0 ? index : index + 1);
    sectInd = AdocAddSection(sectName, buffer);
    if (sectInd < 0)
      report = "Error adding section to autodoc";
  }
  if (report.IsEmpty() && KStoreADOC::SetValuesFromExtra(imBuf->mImage, sectName, 0))
    report = "Error putting metadata into autodoc structure";
  if (report.IsEmpty() && CMD_IS(IMAGEMETADATATOVAR)) {
    if (report.IsEmpty() && AdocPrintToString(buffer, 20000, 1)) {
      report = "Error converting autodoc to string";
    } else {
      bufStr = buffer;
      mStrItems[2].MakeUpper();
      SetVariable(mStrItems[2], bufStr.Trim(" \r\n"), VARTYPE_REGULAR, 0, false, &report);
    }
  } else if (report.IsEmpty()) {
    if (AdocGetString(sectName, 0, mStrItems[2], &valString)) {
      report = "Key " + mStrItems[2] +
        " does not exist in the metadata autodoc structure";
    } else {
      mLogRpt = "Value of " + mStrItems[2] + " is: " + CString(valString);
      if (CMD_IS(REPORTMETADATASTRING)) {
        SetOneReportedValue(&mStrItems[3], CString(valString), 1);
      } else {
        mParamIO->ParseString(valString, &mStrItems[9], MAX_MACRO_TOKENS - 10);
        for (index = 0; index < 6; index++) {
          if (mStrItems[9 + index].IsEmpty())
            break;
          SetOneReportedValue(&mStrItems[3], mStrItems[9 + index], index + 1);
        }
      }
    }
  }
  AdocClear(adocInd);
  AdocReleaseMutex();
  if (!report.IsEmpty())
    ABORT_LINE(report + " for line:\n\n");
  return 0;
}

// LocalVar
int CMacCmd::LocalVar(void)
{
  CString report;
  int index, index2;

  for (index = 1; index < MAX_MACRO_TOKENS && !mItemEmpty[index]; index++) {
    index2 = LocalVarAlreadyDefined(mStrItems[index], mStrLine);
    if (index2 > 0)
      return 1;
    if (!index2 && SetVariable(mStrItems[index], "0",
      VARTYPE_LOCAL + VARTYPE_ADD_FOR_NUM, mCurrentMacro, true, &report))
      ABORT_LINE(report + " in script line: \n\n");
  }
  return 0;
}

// LocalLoopIndexes
int CMacCmd::LocalLoopIndexes(void)
{
  if (!mItemEmpty[1])
    ABORT_NOLINE("LocalLoopIndexes does not take any values; it cannot be turned off");
  if (mBlockLevel > 0)
    ABORT_NOLINE("LocalLoopIndexes cannot be used inside of a loop");
  mLoopIndsAreLocal = true;
  return 0;
}

// Plugin
int CMacCmd::Plugin(void)
{
  CString report;
  double delISX, delISY, delX, delY;
  int index, index2;

  SubstituteVariables(&mStrLine, 1, mStrLine);
  delX = mWinApp->mPluginManager->ExecuteCommand(mStrLine, mItemInt, mItemDbl,
    mItemEmpty, report, delY, delISX, delISY, index2, index);
  if (index) {
    AbortMacro();
    return 1;
  }
  mStrCopy = "";
  if (index2 == 3) {
    SetReportedValues(delX, delY, delISX, delISY);
    mStrCopy.Format(" and it returned %6g, %6g, %6g", delY, delISX, delISY);
  } else if (index == 2) {
    SetReportedValues(delX, delY, delISX);
    mStrCopy.Format(" and it returned %6g, %6g", delY, delISX);
  } else if (index == 1) {
    SetReportedValues(delX, delY);
    mStrCopy.Format(" and it returned %6g", delY);
  } else
    SetReportedValues(delX);
  report += mStrCopy;
  mWinApp->AppendToLog(report, mLogAction);
  return 0;
}

// ListPluginCalls
int CMacCmd::ListPluginCalls(void)
{
  mWinApp->mPluginManager->ListCalls();
  return 0;
}

// FlashDisplay
int CMacCmd::FlashDisplay(void)
{
  double delX;
  int index, index2, i, ix0;

  delX = 0.3;
  index2 = 4;
  i = RGB(192, 192, 0);
  if (mItemInt[1] > 0)
    index2 = mItemInt[1];
  if (!mItemEmpty[2] && mItemDbl[2] >= 0.001)
    delX = mItemDbl[2];
  if (!mItemEmpty[3] && mStrItems[3].GetLength() == 6)
    if (sscanf(mStrItems[3], "%x", &ix0) == 1)
      i = ((ix0 & 0xFF0000) >> 16) | (ix0 & 0x00FF00) | ((ix0 & 0x0000FF) << 16);
  if (delX * index2 > 3)
    ABORT_LINE("Flashing duration is too long in line:\r\n\r\n");
  for (index = 0; index < index2; index++) {
    mWinApp->mMainView->SetFlashNextDisplay(true);
    mWinApp->mMainView->SetFlashColor(i);
    mWinApp->mMainView->DrawImage();
    Sleep(B3DNINT(1000. *delX));
    mWinApp->mMainView->DrawImage();
    if (index < index2 - 1)
      Sleep(B3DNINT(1000. *delX));
  }
  return 0;
}

// TiltUp, U
int CMacCmd::TiltUp(void)
{
  double delISX, delISY;

  // For tilting, if stage is ready, do the action; otherwise back up the index
  if (mScope->StageBusy() <= 0) {
    SetIntensityFactor(1);
    SetupStageRestoreAfterTilt(&mStrItems[1], delISX, delISY);
    mScope->TiltUp(delISX, delISY);
    mMovedStage = true;
    mTestTiltAngle = true;
  } else
    mLastIndex = mCurrentIndex;
  return 0;
}

// TiltDown, D
int CMacCmd::TiltDown(void)
{
  double delISX, delISY;

  if (mScope->StageBusy() <= 0) {
    SetIntensityFactor(-1);
    SetupStageRestoreAfterTilt(&mStrItems[1], delISX, delISY);
    mScope->TiltDown(delISX, delISY);
    mMovedStage = true;
    mTestTiltAngle = true;
  } else
    mLastIndex = mCurrentIndex;
  return 0;
}

// TiltTo, TiltBy
int CMacCmd::TiltTo(void)
{
  CString report;
  double delISX, delISY;
  double angle = mItemFlt[1];
  if (CMD_IS(TILTBY))
    angle += mScope->GetTiltAngle();
  if (fabs(angle) > mScope->GetMaxTiltAngle() + 0.05) {
    report.Format("Attempt to tilt too high in script, to %.1f degrees, in line:\n\n",
      angle);
    ABORT_LINE(report);
  }
  if (mScope->StageBusy() <= 0) {
    SetupStageRestoreAfterTilt(&mStrItems[2], delISX, delISY);
    mScope->TiltTo(angle, delISX, delISY);
    mMovedStage = true;
    mTestTiltAngle = true;
  }
  else
    mLastIndex = mCurrentIndex;
  return 0;
}

// SetTiltIncrement
int CMacCmd::SetTiltIncrement(void)
{
  if (mWinApp->mTSController->StartedTiltSeries())
    ABORT_LINE("You cannot change tilt increment after a tilt series is started");
  if (mItemFlt[1] < 0.02 || mItemFlt[1] > 60)
    ABORT_LINE("Tilt increment must be between 0.02 and 60 for line:\n\n");
  mScope->SetIncrement(mItemFlt[1]);
  if (!mItemEmpty[2])
    mScope->SetCosineTilt(mItemInt[2] != 0);
  return 0;
}

// SetStageBAxis
int CMacCmd::SetStageBAxis(void)
{
  if (mScope->StageBusy() <= 0) {
    if (!mScope->SetStageBAxis(mItemDbl[1])) {
      AbortMacro();
      return 1;
    }
    mMovedStage = true;
  } else
    mLastIndex = mCurrentIndex;
  return 0;
}

// OpenDECameraCover
int CMacCmd::OpenDECameraCover(void)
{
  mOpenDE12Cover = true;
  mWinApp->mDEToolDlg.Update();
  return 0;
}

// View, V
int CMacCmd::View(void)
{
  mCamera->InitiateCapture(VIEW_CONSET);
  mTestScale = true;
  return 0;
}

// Focus, F
int CMacCmd::Focus(void)
{
  mCamera->InitiateCapture(FOCUS_CONSET);
  mTestScale = true;
  return 0;
}

// Trial, T
int CMacCmd::Trial(void)
{
  mCamera->InitiateCapture(TRIAL_CONSET);
  mTestScale = true;
  return 0;
}

// Record, R
int CMacCmd::Record(void)
{
  mCamera->InitiateCapture(RECORD_CONSET);
  mTestScale = true;
  return 0;
}

// Preview, L
int CMacCmd::Preview(void)
{
  mCamera->InitiateCapture(PREVIEW_CONSET);
  mTestScale = true;
  return 0;
}

// Search
int CMacCmd::Search(void)
{
  mCamera->InitiateCapture(SEARCH_CONSET);
  mTestScale = true;
  return 0;
}

// Montage, M, PreCookMontage
int CMacCmd::Montage(void)
{
  BOOL truth;
  int index;

  truth = CMD_IS(PRECOOKMONTAGE);
  index = MONT_NOT_TRIAL;
  if (truth)
    index = MONT_TRIAL_PRECOOK;
  else if (mItemInt[1] != 0)
    index = MONT_TRIAL_IMAGE;
  if (!mWinApp->Montaging())
    ABORT_NOLINE("The script contains a montage statement and \n"
      "montaging is not activated");
  if (mWinApp->mMontageController->GetRunningMacro())
    ABORT_NOLINE("The script contains a montage statement but \n"
      "it was run during a montage");
  if (mWinApp->mMontageController->StartMontage(index, false,
    (float)(truth ? mItemDbl[1] : 0.), (truth && !mItemEmpty[2]) ? mItemInt[2] : 0,
    truth && mItemInt[3] != 0,
    (truth && !mItemEmpty[4]) ? mItemFlt[4] : 0.f))
    AbortMacro();
  mTestMontError = !truth;
  mTestScale = !truth;
  return 0;
}

// ReportMontagePiece
int CMacCmd::ReportMontagePiece(void)
{
  int xPc, yPc, ixPc, iyPc;
  if (mWinApp->mMontageController->GetCurrentPieceInfo(mItemInt[1] != 0, xPc, yPc, ixPc,
    iyPc))
    ABORT_NOLINE("ReportMontagePiece can be used only in a script run from a montage");
  SetRepValsAndVars(2, xPc + 1, yPc + 1, ixPc + 1, iyPc + 1, mMontP->xNframes, 
    mMontP->yNframes);
  mLogRpt.Format("%s montage piece: %d %d at %d %d, montage is %d x %d", mItemInt[1] ? 
    "next" : "current", xPc + 1, yPc + 1, ixPc + 1, iyPc + 1, mMontP->xNframes,
    mMontP->yNframes);
  return 0;
}

// SetRedoMontageXCorrs
int CMacCmd::SetRedoMontageXCorrs(void)
{
  mWinApp->mMontageController->SetRedoCorrOnRead(mItemInt[1] != 0);
  return 0;
}

// OppositeTrial, OppositeFocus, OppositeAutoFocus
int CMacCmd::OppositeTrial(void)
{
  int index;

  if (!mWinApp->LowDoseMode())
    ABORT_LINE("Low dose mode needs to be on to use opposite area in statement: \n\n");
  if (CMD_IS(OPPOSITEAUTOFOCUS) && !mFocusManager->FocusReady())
    ABORT_NOLINE("because autofocus not calibrated");
  if (mCamera->OppositeLDAreaNextShot())
    ABORT_LINE("You can not use opposite areas when Balance Shifts is on in "
    "statement: \n\n");
  mTestScale = true;
  if (CMD_IS(OPPOSITEAUTOFOCUS)) {
    mFocusManager->SetUseOppositeLDArea(true);
    index = mItemEmpty[1] ? 1 : mItemInt[1];
    mFocusManager->AutoFocusStart(index);
  } else if (CMD_IS(OPPOSITETRIAL))
    mCamera->InitiateCapture(2);
  else
    mCamera->InitiateCapture(1);
  return 0;
}

// AcquireToMatchBuffer
int CMacCmd::AcquireToMatchBuffer(void)
{
  CString report;
  double v1;
  int index, index2, ix0, ix1, iy0, iy1, sizeX, sizeY, binning;

  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report))
    ABORT_LINE(report);
  index2 = mImBufs[index].mConSetUsed;
  binning = mImBufs[index].mBinning;
  if (index2 < 0 || binning <= 0)
    ABORT_LINE("Either the parameter set or the binning is not specified in the buffer"
      " for:\n\n");
  if (mImBufs[index].mCamera != mCurrentCam)
    ABORT_LINE("Current camera is no longer the same as was used to acquire the image "
      "for:\n\n");
  if (index2 == MONTAGE_CONSET)
    index2 = RECORD_CONSET;
  if (index2 == TRACK_CONSET)
    index2 = TRIAL_CONSET;
  mImBufs[index].mImage->getSize(mCropXafterShot, mCropYafterShot);
  sizeX = mCropXafterShot;
  sizeY = mCropYafterShot;
  v1 = 1.05;

  // Get the parameters to achieve a size that is big enough
  for (;;) {
    mCamera->CenteredSizes(sizeX, mCamParams->sizeX, mCamParams->moduloX, ix0, ix1,
      sizeY, mCamParams->sizeY, mCamParams->moduloY, iy0, iy1, binning);
    if (sizeX >= mCamParams->sizeX / binning && sizeY >= mCamParams->sizeY / binning)
      break;
    if (sizeX >= mCropXafterShot && sizeY >= mCropYafterShot)
      break;
    if (sizeX < mCropXafterShot)
      sizeX = B3DMIN((int)(v1 * mCropXafterShot), mCamParams->sizeX / binning);
    if (sizeY < mCropYafterShot)
      sizeY = B3DMIN((int)(v1 * mCropYafterShot), mCamParams->sizeY / binning);
    v1 *= 1.05;
  }

  // Save the control set on top of stack and make modifications
  mConsetsSaved.push_back(mConSets[index2]);
  mConsetNums.push_back(index2);
  mRestoreConsetAfterShot = true;
  mConSets[index2].binning = binning;
  mConSets[index2].left = ix0 * binning;
  mConSets[index2].right = ix1 * binning;
  mConSets[index2].top = iy0 * binning;
  mConSets[index2].bottom = iy1 * binning;
  mConSets[index2].exposure = mImBufs[index].mExposure;
  mConSets[index2].K2ReadMode = mImBufs[index].mK2ReadMode;
  mConSets[index2].mode = SINGLE_FRAME;
  mConSets[index2].alignFrames = 0;
  mConSets[index2].saveFrames = 0;

  // Cancel crop if it isn't needed
  if (sizeX == mCropXafterShot && sizeY == mCropYafterShot)
    mCropXafterShot = -1;
  mCamera->InitiateCapture(index2);
  return 0;
}

// StepFocusNextShot
int CMacCmd::StepFocusNextShot(void)
{
  double delX, delY;

  delX = 0.;
  delY = 0.;
  if (!mItemEmpty[4]) {
    delX = mItemDbl[3];
    delY = mItemDbl[4];
  }
  if (mItemDbl[1] <= 0. || delX < 0. || (delX > 0. && delX < mItemDbl[1]) ||
    (!delX && !mItemEmpty[3]))
    ABORT_LINE("Negative time, times out of order, or odd number of values in "
    "statement: \n\n");
  mCamera->QueueFocusSteps(mItemFlt[1], mItemDbl[2], (float)delX, delY);
  return 0;
}

// SmoothFocusNextShot
int CMacCmd::SmoothFocusNextShot(void)
{
  if (fabs(mItemDbl[1] - mItemDbl[2]) < 0.1)
    ABORT_LINE("Focus change must be as least 0.1 micron in statement: \n\n");
  mCamera->QueueFocusSteps(0., mItemDbl[1], 0., mItemDbl[2]);
  return 0;
}

// DeferStackingNextShot
int CMacCmd::DeferStackingNextShot(void)
{
  if (!IS_BASIC_FALCON2(mCamParams))
    ABORT_NOLINE("Deferred stacking is available only for Falcon2 with old interface");
  mCamera->SetDeferStackingFrames(true);
  return 0;
}

// EarlyReturnNextShot
int CMacCmd::EarlyReturnNextShot(void)
{
  int index;

  index = mItemInt[1];
  if (index < 0)
    index = 65535;
  if (mCamera->SetNextAsyncSumFrames(index, mItemInt[2] > 0, mItemInt[3] != 0)) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// GetDeferredSum
int CMacCmd::GetDeferredSum(void)
{
  if (mCamera->GetDeferredSum()) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// FrameThresholdNextShot
int CMacCmd::FrameThresholdNextShot(void)
{
  float backlashX, backlashY;

  mCamera->SetNextFrameSkipThresh(mItemFlt[1]);
  if (!mItemEmpty[2]) {
    backlashX = mItemFlt[2];
    backlashY = mItemEmpty[3] ? backlashX : mItemFlt[3];
    if (backlashX >= 1. || backlashX < 0. || backlashY >= 1. || backlashY < 0)
      ABORT_LINE("Partial frame thresholds for Alignframes must be >= 0 and < 1 for "
        "line:\n\n");
    mCamera->SetNextPartialThresholds(backlashX, backlashY);
  }
  if (!mItemEmpty[4]) {
    if (mItemDbl[4] < 0 || mItemDbl[4] >= 1.)
      ABORT_LINE("The relative frame thresholds must be >= 0 and < 1 for line:\n\n");
    mCamera->SetNextRelativeThresh(mItemFlt[4]);
  }
  return 0;
}

// QueueFrameTiltSeries, FrameSeriesFromVar
int CMacCmd::QueueFrameTiltSeries(void)
{
  CString report;
  ScaleMat aMat, IStoBS; //, bMat;
  double delay, delISX, delISY, delX, delY;
  float postISdelay = 0.;  // , pixel;
  int index, index2, ix0, ix1, iy0, iy1, sizeX, sizeY;
  Variable *var;
  CString *valPtr;
  FloatVec openTime, tiltToAngle, waitOrInterval, focusChange, deltaISX, deltaISY;
  FloatVec deltaBeamX, deltaBeamY;

  // Get matrix for image shift conversion
  index = mWinApp->LowDoseMode() ? mLdParam[RECORD_CONSET].magIndex :
    mScope->GetMagIndex();
  aMat = MatMul(MatInv(mShiftManager->CameraToSpecimen(index)),
    mShiftManager->CameraToIS(index));
  IStoBS = mShiftManager->GetBeamShiftCal(index);

  // Set up series based on specifications
  if (CMD_IS(QUEUEFRAMETILTSERIES)) {
    delay = mItemEmpty[4] ? 0. : mItemDbl[4];
    if (!mItemEmpty[9] && !aMat.xpx)
      ABORT_LINE("There is no calibration needed to convert the image shift values "
        "for:\n\n");
    for (index = 0; index < mItemInt[3]; index++) {
      tiltToAngle.push_back((float)(mItemDbl[1] + mItemDbl[2] * index));
      if (!mItemEmpty[5])
        openTime.push_back(mItemFlt[5]);
      if (!mItemEmpty[6])
        waitOrInterval.push_back(mItemFlt[6]);
      if (!mItemEmpty[7])
        focusChange.push_back((float)(mItemDbl[7] *index));
      if (!mItemEmpty[9]) {
        deltaISX.push_back((float)((mItemDbl[8] *aMat.xpx + mItemDbl[9] * aMat.xpy) *
          index));
        deltaISY.push_back((float)((mItemDbl[8] *aMat.ypx + mItemDbl[9] * aMat.ypy) *
          index));
      }
    }

  } else {
    if ((mItemInt[2] & 16) && !aMat.xpx)
      ABORT_LINE("There is no calibration needed to convert the image shift values "
        "for:\n\n");

    // Beam shift is in raw units by request; commented out code will convert them to 
    // microns in the camera coordinate system
    /*if (mItemInt[2] & 32) {
      bMat = MatMul(mShiftManager->CameraToIS(index), IStoBS);
      pixel = mShiftManager->GetPixelSize(mCurrentCam, index);
      if (!bMat.xpx)
        ABORT_LINE("There is no calibration needed to convert the beam shift values "
          "for:\n\n");
    }*/

    if (mItemInt[2] & 16 && JEOLscope && !IStoBS.xpx) {
      ABORT_LINE("There is no calibration for getting beam shifts from the image shifts");
    }

    // Or pull the values out of the big variable array: figure out how many per step
    delay = mItemEmpty[3] ? 0. : mItemDbl[3];
    postISdelay = mItemEmpty[4] ? 0.f : mItemFlt[4];
    var = LookupVariable(mItem1upper, index2);
    if (!var)
      ABORT_LINE("The variable " + mStrItems[1] + " is not defined in line:\n\n");
    sizeX = 0;
    if (mItemInt[2] > 63 || mItemInt[2] < 1)
      ABORT_LINE("The entry with flags must be between 1 and 63 in line:\n\n");
    if (mItemInt[2] & 1)
      sizeX++;
    if (mItemInt[2] & 2)
      sizeX++;
    if (mItemInt[2] & 4)
      sizeX++;
    if (mItemInt[2] & 8)
      sizeX++;
    if (mItemInt[2] & 16)
      sizeX += 2;
    if (mItemInt[2] & 32)
      sizeX += 2;
    if (var->rowsFor2d) {
      sizeY = (int)var->rowsFor2d->GetSize();
      for (ix0 = 0; ix0 < sizeY; ix0++) {
        ArrayRow& tempRow = var->rowsFor2d->ElementAt(ix0);
        if (tempRow.numElements < sizeX) {
          report.Format("Row %d or the 2D array %s has only %d elements, not the %d"
            " required", ix0, mItem1upper, tempRow.numElements, sizeX);
          ABORT_NOLINE(report);
        }
      }
    } else {
      sizeY = var->numElements / sizeX;
      valPtr = &var->value;
      if (var->numElements % sizeX) {
        report.Format("Variable %s has %d elements, not divisible by the\n"
          "%d values per step implied by the flags entry of %d", mStrItems[1],
          var->numElements, sizeX, mItemInt[2]);
        ABORT_NOLINE(report);
      }
    }

    // Load the vectors
    iy0 = 1;
    for (ix0 = 0; ix0 < sizeY; ix0++) {
      if (var->rowsFor2d) {
        ArrayRow& tempRow = var->rowsFor2d->ElementAt(ix0);
        valPtr = &tempRow.value;
        iy0 = 1;
      }
      if (mItemInt[2] & 1) {
        FindValueAtIndex(*valPtr, iy0, ix1, iy1);
        report = valPtr->Mid(ix1, iy1 - ix1);
        tiltToAngle.push_back((float)atof((LPCTSTR)report));
        iy0++;
      }
      if (mItemInt[2] & 2) {
        FindValueAtIndex(*valPtr, iy0, ix1, iy1);
        report = valPtr->Mid(ix1, iy1 - ix1);
        openTime.push_back((float)atof((LPCTSTR)report));
        iy0++;
      }
      if (mItemInt[2] & 4) {
        FindValueAtIndex(*valPtr, iy0, ix1, iy1);
        report = valPtr->Mid(ix1, iy1 - ix1);
        waitOrInterval.push_back((float)atof((LPCTSTR)report));
        iy0++;
      }
      if (mItemInt[2] & 8) {
        FindValueAtIndex(*valPtr, iy0, ix1, iy1);
        report = valPtr->Mid(ix1, iy1 - ix1);
        focusChange.push_back((float)atof((LPCTSTR)report));
        iy0++;
      }
      if (mItemInt[2] & 16) {
        FindValueAtIndex(*valPtr, iy0, ix1, iy1);
        report = valPtr->Mid(ix1, iy1 - ix1);
        delX = atof((LPCTSTR)report);
        iy0++;
        FindValueAtIndex(*valPtr, iy0, ix1, iy1);
        report = valPtr->Mid(ix1, iy1 - ix1);
        delY = atof((LPCTSTR)report);
        ApplyScaleMatrix(aMat, (float)delX, (float)delY, delISX, delISY);
        deltaISX.push_back((float)(delISX));
        deltaISY.push_back((float)(delISX));
        iy0++;
        if (JEOLscope) {
          ApplyScaleMatrix(IStoBS, (float)delISX, (float)delISY, delX,
            delY);
          deltaBeamX.push_back((float)delX);
          deltaBeamY.push_back((float)delY);
        }
      }
      if (mItemInt[2] & 32) {
        FindValueAtIndex(*valPtr, iy0, ix1, iy1);
        report = valPtr->Mid(ix1, iy1 - ix1);
        delX = atof((LPCTSTR)report);  // / pixel;
        iy0++;
        FindValueAtIndex(*valPtr, iy0, ix1, iy1);
        report = valPtr->Mid(ix1, iy1 - ix1);
        delY = atof((LPCTSTR)report); // / pixel;
        if (JEOLscope && (mItemInt[2] & 16)) {
          ix0 = (int)deltaBeamX.size() - 1;
          deltaBeamX[ix0] += (float)delX;
          deltaBeamY[ix0] += (float)delY;
        } else {
          deltaBeamX.push_back((float)delX);
          deltaBeamY.push_back((float)delY);
        }
        //deltaBeamX.push_back((float)(delX * bMat.xpx + delY * bMat.xpy));
        //deltaBeamY.push_back((float)(delX * bMat.ypx + delY * bMat.ypy));
        iy0++;
      }
    }
  }

  // Queue it: This does all the error checking
  if (mCamera->QueueTiltSeries(openTime, tiltToAngle, waitOrInterval, focusChange,
    deltaISX, deltaISY, deltaBeamX, deltaBeamY, (float)delay, postISdelay)) {
      AbortMacro();
      return 1;
  }
  return 0;
}

// SetFrameSeriesParams
int CMacCmd::SetFrameSeriesParams(void)
{
  BOOL truth;
  float backlashX;

  truth = mItemInt[1] != 0;
  backlashX = 0.;
  if (!mItemEmpty[2])
    backlashX = mItemFlt[2];
  if (!mItemEmpty[3] && mItemEmpty[4] ||
    !BOOL_EQUIV(fabs(mItemDbl[3]) > 9000., fabs(mItemDbl[4]) > 9000.))
    ABORT_LINE("A Y position to restore must be entered when X is on line:\n\n");
  if (mCamera->SetFrameTSparams(truth, backlashX,
    (mItemEmpty[3] || fabs(mItemDbl[3]) > 9000.) ? EXTRA_NO_VALUE : mItemDbl[3],
    (mItemEmpty[4] || fabs(mItemDbl[4]) > 9000.) ? EXTRA_NO_VALUE : mItemDbl[4]))
    ABORT_LINE("The FEI scope plugin version is not new enough to support variable"
      " speed for line:\n\n");
  if (!mItemEmpty[5]) {
    if (mItemInt[5] <= 0 || mItemInt[5] > 99)
      ABORT_LINE("Minimum gap size for recognizing new tilts must be between 1 and 99"
        " for line:\n\n");
    mCamera->SetNextMinTiltGap(mItemInt[5]);
  }
  if (!mItemEmpty[6]) {
    if (mItemInt[6] < 0 || mItemInt[6] > 30)
      ABORT_LINE("Number of frames to drop from tilt sums must be between 0 and 30"
        " for line:\n\n");
    mCamera->SetNextDropFromTiltSum(mItemInt[6]);
  }
  return 0;
}

// WriteFrameSeriesAngles
int CMacCmd::WriteFrameSeriesAngles(void)
{
  CString report;
  int index;
  FloatVec *angles = mCamera->GetFrameTSactualAngles();
  IntVec *startTime = mCamera->GetFrameTSrelStartTime();
  IntVec *endTime = mCamera->GetFrameTSrelEndTime();
  float frame = mCamera->GetFrameTSFrameTime();
  if (!angles->size())
    ABORT_NOLINE("There are no angles available from a frame tilt series");
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  CString message = "Error opening file ";
  CStdioFile *csFile = NULL;
  try {
    csFile = new CStdioFile(mStrCopy, CFile::modeCreate | CFile::modeWrite |
      CFile::shareDenyWrite);
    message = "Writing angles to file ";
    for (index = 0; index < (int)angles->size(); index++) {
      report.Format("%7.2f", angles->at(index));
      if (index < (int)startTime->size())
        mStrCopy.Format(" %5d %5d\n", B3DNINT(0.001 * startTime->at(index) / frame),
          B3DNINT(0.001 *endTime->at(index) / frame));
      else
        mStrCopy = "\n";
      csFile->WriteString((LPCTSTR)report + mStrCopy);
    }
  }
  catch (CFileException * perr) {
    perr->Delete();
    ABORT_NOLINE(message + mStrCopy);
  }
  delete csFile;
  return 0;
}

// ReportFrameTiltSum
int CMacCmd::ReportFrameTiltSum(void)
{
  BOOL truth;
  int index, index2, ix0, ix1;
  float backlashX;

  truth = mCamera->GetTiltSumProperties(index, index2, backlashX, ix0, ix1);
  if (truth) {
    mLogRpt = "There are no more tilt sums available";
    SetRepValsAndVars(1, 1, 0.);
  } else {
    mLogRpt.Format("Tilt sum from %.1f deg, index %d, %d frames from %d to %d",
      backlashX, index, index2, ix0, ix1);
    SetRepValsAndVars(1, 0, backlashX, index, index2, ix0, ix1);
  }
  return 0;
}

// ModifyFrameTSShifts
int CMacCmd::ModifyFrameTSShifts(void)
{
  mCamera->ModifyFrameTSShifts(mItemInt[1], mItemFlt[2], mItemFlt[3]);
  return 0;
}

// ReplaceFrameTSFocus, ReplaceFrameTSShifts
int CMacCmd::ReplaceFrameTSFocus(void)
{
  CString report;
  BOOL truth;
  int index, index2, ix1, iy0, iy1;
  Variable *var;
  CString *valPtr;
  FloatVec newFocus, newSecond, *newVec = &newFocus;
  truth = CMD_IS(REPLACEFRAMETSSHIFTS);
  for (index = 1; index < (truth ? 3 : 2); index++) {
    mStrCopy = mStrItems[index];
    mStrCopy.MakeUpper();
    var = LookupVariable(mStrCopy, index2);
    if (!var)
      ABORT_LINE("The variable " + mStrItems[index] + " is not defined in line:\n\n");
    if (var->rowsFor2d)
      ABORT_LINE("The variable " + mStrItems[index] + " should not be a 2D array for "
        "line:\n\n");
    valPtr = &var->value;
    for (iy0 = 0; iy0 < var->numElements; iy0++) {
      FindValueAtIndex(*valPtr, iy0, ix1, iy1);
      report = valPtr->Mid(ix1, iy1 - ix1);
      newVec->push_back((float)atof((LPCTSTR)report));
    }
    if (truth)
      newVec = &newSecond;
  }
  if (truth)
    index2 = mCamera->ReplaceFrameTSShifts(newFocus, newSecond);
  else
    index2 = mCamera->ReplaceFrameTSFocusChange(newFocus);
  if (index2)
    ABORT_LINE("The variable does not have the same number of changes as the"
      " original list of changes for line:\n\n");
  return 0;
}

// RetractCamera
int CMacCmd::RetractCamera(void)
{

  // Back up index unless camera ready
  if (mCamera->CameraBusy())
    mLastIndex = mCurrentIndex;
  else
    mCamera->RetractAllCameras();
  return 0;
}

// RecordAndTiltUp, RecordAndTiltDown, RecordAndTiltTo
int CMacCmd::RecordAndTiltUp(void)
{
  BOOL truth;
  bool doBack;
  double delISX;
  int index;
  float backlashX;

  if (!mCamera->PostActionsOK(&mConSets[RECORD_CONSET]))
    ABORT_LINE("Post-exposure actions are not allowed for the current camera"
      " for line:\n\n");
  double increment = mScope->GetIncrement();
  doBack = false;
  delISX = mScope->GetTiltAngle();
  index = 1;
  if (CMD_IS(RECORDANDTILTTO)) {
    index = 3;
    increment = mItemDbl[1] - delISX;
    if (!mItemEmpty[2] && mItemDbl[2]) {
      doBack = true;
      backlashX = (float)fabs(mItemDbl[2]);
      if (increment < 0)
        backlashX = -backlashX;
    }
  }
  int delay = mShiftManager->GetAdjustedTiltDelay(increment);
  if (CMD_IS(RECORDANDTILTDOWN)) {
    increment = -increment;
    SetIntensityFactor(-1);
  } else if (CMD_IS(RECORDANDTILTUP)) {
    SetIntensityFactor(1);
  }

  StageMoveInfo smiRAT;
  smiRAT.axisBits = axisA;
  smiRAT.alpha = delISX + increment;
  smiRAT.backAlpha = backlashX;
  truth = SetupStageRestoreAfterTilt(&mStrItems[index], smiRAT.x, smiRAT.y);
  mCamera->QueueStageMove(smiRAT, delay, doBack, truth);
  mCamera->InitiateCapture(RECORD_CONSET);
  mTestScale = true;
  mMovedStage = true;
  mTestTiltAngle = true;
  return 0;
}

// ArePostActionsEnabled
int CMacCmd::ArePostActionsEnabled(void)
{
  BOOL truth, doShift;

  truth = mWinApp->ActPostExposure();
  doShift = mCamera->PostActionsOK(&mConSets[RECORD_CONSET]);
  mLogRpt.Format("Post-exposure actions %s allowed %sfor this camera%s",
    truth ? "ARE" : "are NOT", doShift ? "in general " : "",
    doShift ? " but ARE for Records currently" : "");
  SetRepValsAndVars(1, truth ? 1. : 0., doShift ? 1 : 0.);
  return 0;
}

// TiltDuringRecord
int CMacCmd::TiltDuringRecord(void)
{
  CString report;
  double delX;

  delX = mItemEmpty[3] ? 0. : mItemDbl[3];
  if (mCamera->QueueTiltDuringShot(mItemDbl[1], mItemInt[2], delX)) {
    report.Format("Moving the stage with variable speed requires\n"
      "FEI plugin version %d (and same for server, if relevant)\n"
      "You only have version %d and this line cannot be used:\n\n",
      FEI_PLUGIN_STAGE_SPEED, mScope->GetPluginVersion());
    ABORT_LINE(report);
  }
  mCamera->InitiateCapture(3);
  mMovedStage = true;
  mTestTiltAngle = true;
  return 0;
}

// SetDoseAdjustmentFactor
int CMacCmd::SetDoseAdjustmentFactor(void)
{
  if (mItemFlt[1] < 0.01 || mItemFlt[1] > 50.)
    ABORT_LINE("Factor must be between 0.01 and 50 for line:\n\n");
  mCamera->SetDoseAdjustmentFactor(mItemFlt[1]);
  mNumStatesToRestore++;
  return 0;
}

// TestNextMultiShot
int CMacCmd::TestNextMultiShot(void)
{
  if (mItemInt[1] < 0 || mItemInt[1] > 2)
    ABORT_LINE("The value must be between 0 and 2 in line:\n\n");
  mTestNextMultiShot = mItemInt[1];
  return 0;
}

// MultipleRecords
int CMacCmd::MultipleRecords(void)
{
  BOOL doSave;
  int doEarly, index2, ix0, ix1, iy1, numEarly, maxFlags = MULTI_FORCE_CUSTOM * 2 - 1;

  mNavHelper->UpdateMultishotIfOpen();
  MultiShotParams *msParams = mNavHelper->GetMultiShotParams();
  doEarly = (mItemEmpty[6] || mItemInt[6] < -8) ? msParams->doEarlyReturn : mItemInt[6];
  numEarly = (mItemEmpty[7] || mItemInt[7] < -8) ? msParams->numEarlyFrames : mItemInt[7];
  if (!mCamParams->K2Type)
    doEarly = 0;
  index2 = msParams->inHoleOrMultiHole | (mTestNextMultiShot << 2);
  if (!mItemEmpty[9] && mItemInt[9] > -9) {
    index2 = mItemInt[9];
    if (index2 < 1 || index2 > maxFlags) {
      mStrCopy.Format("The ninth entry for doing within holes or\n"
        "in multiple holes must be between 1 and %d in line:\n\n", maxFlags);
      ABORT_LINE(mStrCopy);
    }
  }
  doSave = (doEarly != 2 || numEarly != 0) ? msParams->saveRecord : false;
  ix0 = msParams->doSecondRing ? 1 : 0;
  if (!mItemEmpty[10] && mItemInt[10] > -9)
    ix0 = mItemInt[10] ? 1 : 0;
  ix1 = 0;
  if (ix0)
    ix1 = (mItemEmpty[11] || mItemInt[11] < -8) ? msParams->numShots[1] : mItemInt[11];
  iy1 = mWinApp->mParticleTasks->StartMultiShot(
    (mItemEmpty[1] || mItemInt[1] < -8) ? msParams->numShots[0] : mItemInt[1],
    (mItemEmpty[2] || mItemInt[2] < -8) ? msParams->doCenter : mItemInt[2],
    (mItemEmpty[3] || mItemDbl[3] < -8.) ? msParams->spokeRad[0] : mItemFlt[3], ix1,
    (mItemEmpty[12] || mItemDbl[12] < -8.) ? msParams->spokeRad[1] : mItemFlt[12],
    (mItemEmpty[4] || mItemDbl[4] < -8.) ? msParams->extraDelay : mItemFlt[4],
    (mItemEmpty[5] || mItemInt[5] < -8) ? doSave : mItemInt[5] != 0, doEarly, numEarly,
    (mItemEmpty[8] || mItemInt[8] < -8) ? msParams->adjustBeamTilt : mItemInt[8] != 0,
    index2);
  if (iy1 > 0) {
    AbortMacro();
    return 1;
  }
  SetReportedValues(-iy1);
  return 0;
}

// RotateMultiShotPattern
int CMacCmd::RotateMultiShotPattern(void)
{
  int index;

  mNavHelper->UpdateMultishotIfOpen();
  index = mNavHelper->RotateMultiShotVectors(mNavHelper->GetMultiShotParams(),
    mItemFlt[1], mItemInt[2] != 0);
  if (index)
    ABORT_LINE(index > 1 ? "No image shift to specimen transformation available "
      "for line:\n\n" : "Selected pattern is not defined for line:/n/n");
  if (mNavHelper->mMultiShotDlg)
    mNavHelper->mMultiShotDlg->UpdateSettings();
  return 0;
}

// OpenMultiShotFiles
int CMacCmd::OpenMultiShotFiles(void)
{
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  if (mWinApp->mParticleTasks->OpenSeparateMultiFiles(mStrCopy)) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// CloseMultiShotFiles
int CMacCmd::CloseMultiShotFiles(void)
{
  mWinApp->mParticleTasks->CloseSeparateMultiFiles();
  return 0;
}

// SetCustomHoleShifts, SetRegularHoleVectors
int CMacCmd::SetCustomHoleShifts(void)
{
  int index;
  MultiShotParams *msParams = mNavHelper->GetMultiShotParams();
  Variable *yvar, *xvar;
  bool custom = CMD_IS(SETCUSTOMHOLESHIFTS);
  if (custom) {
    xvar = LookupVariable(mItem1upper, index);
    if (!xvar)
      ABORT_LINE("The variable " + mStrItems[1] + "is not defined for line:\n\n");
    mItem1upper = mStrItems[2];
    mItem1upper.MakeUpper();
    yvar = LookupVariable(mItem1upper, index);
    if (!yvar)
      ABORT_LINE("The variable " + mStrItems[2] + "is not defined for line:\n\n");
    if (xvar->rowsFor2d || yvar->rowsFor2d)
      ABORT_LINE("Neither variable should be a 2D array for line:\n\n");
    if (xvar->numElements != yvar->numElements)
      ABORT_LINE("The two variables do not have the same number of values for line:\n\n");
    if (mItemEmpty[3] && !msParams->customMagIndex)
      ABORT_LINE("A magnification index must be included because there is none currently "
        "defined for custom holes");
  } else {
    if (mItemEmpty[5] && !msParams->holeMagIndex)
      ABORT_LINE("A magnification index must be included because there is none currently "
        "defined for regular hole vectors");
  }
  index = custom ? 3 : 5;
  if (!mItemEmpty[index] && (mItemInt[index] < 0 || mItemInt[index] >= MAX_MAGS))
    ABORT_LINE("The magnification index is out of range in line:\n\n");
  if (!mItemEmpty[index + 1] && fabs(mItemFlt[index + 1]) > mScope->GetMaxTiltAngle())
    ABORT_LINE("The tilt angle is out of range in line:\n\n");
  if (custom) {
    FillVectorFromArrayVariable(&msParams->customHoleX, NULL, xvar);
    FillVectorFromArrayVariable(&msParams->customHoleY, NULL, yvar);
    if (!mItemEmpty[3] && mItemInt[3])
      msParams->customMagIndex = mItemInt[3];
    if (!mItemEmpty[4])
      msParams->tiltOfCustomHoles = mItemFlt[4];
  } else {
    msParams->holeISXspacing[0] = mItemDbl[1];
    msParams->holeISYspacing[0] = mItemDbl[2];
    msParams->holeISXspacing[1] = mItemDbl[3];
    msParams->holeISYspacing[1] = mItemDbl[4];
    if (!mItemEmpty[5] && mItemInt[5])
      msParams->holeMagIndex = mItemInt[5];
    if (!mItemEmpty[6])
      msParams->tiltOfHoleArray = mItemFlt[6];
  }
  if (mNavHelper->mMultiShotDlg)
    mNavHelper->mMultiShotDlg->UpdateSettings();
  return 0;
}

// SetCustomHoleDefocus
int CMacCmd::SetCustomHoleDefocus(void)
{
  int index, ix0, ix1;
  MultiShotParams *msParams = mNavHelper->GetMultiShotParams();
  Variable *var = LookupVariable(mItem1upper, index);
  if (!var)
    ABORT_LINE("The variable " + mStrItems[1] + "is not defined for line:\n\n");
  if (!msParams->customMagIndex)
    ABORT_LINE("There are no custom holes defined for line:\n\n");
  if (var->rowsFor2d)
    ABORT_LINE("The variable " + mStrItems[1] + " should not be a 2D array for line"
      ":\n\n");
  msParams->customDefocus.clear();
  for (index = 0; index < var->numElements; index++) {
    FindValueAtIndex(var->value, index + 1, ix0, ix1);
    mStrCopy = var->value.Mid(ix0, ix1 - ix0);
    msParams->customDefocus.push_back((float)atof((LPCTSTR)mStrCopy));
  }
  return 0;
}


// ReorderMontageByTilt
int CMacCmd::ReorderMontageByTilt(void)
{
  mStrCopy = "";
  if (mWinApp->mMultiTSTasks->ReorderMontageByTilt(mStrCopy))
    ABORT_NOLINE("Error reordering a montage by tilt angle:\n" + mStrCopy);
  return 0;
}

// AutoAlign, A, AlignTo, ConicalAlignTo
int CMacCmd::AutoAlign(void)
{
  CString report;
  BOOL doShift;
  double delX;
  int index, index2, flags = 0;

  index = 0;
  if (CMD_IS(ALIGNTO)  || CMD_IS(CONICALALIGNTO)) {
    if (ConvertBufferLetter(mStrItems[1], -1, true, index, report))
      ABORT_LINE(report);
  }
  delX = 0;
  doShift = true;
  if (CMD_IS(CONICALALIGNTO)) {
    delX = mItemDbl[2];
    flags = (mItemInt[3] != 0) ? AUTOALIGN_SHOW_CORR : 0;
  }
  if (CMD_IS(ALIGNTO)) {
    if (mItemInt[2])
      doShift = false;
    if (mItemInt[4] > 0)
      flags += AUTOALIGN_FILL_SPOTS;
    if (mItemInt[4] < 0)
      flags += AUTOALIGN_KEEP_SPOTS;
    if (mItemInt[5])
      flags += AUTOALIGN_SHOW_CORR + ((mItemInt[5] & 2) ? AUTOALIGN_SHOW_FILTA : 0) +
      ((mItemInt[5] & 4) ? AUTOALIGN_SHOW_FILTC : 0);
    mDisableAlignTrim = mItemInt[3] != 0;
    if (!mItemEmpty[7]) {
      mImBufs[0].mImage->setShifts(0., 0.);
      mImBufs[index].mImage->setShifts(0., 0.);
    }
  }
  index2 = mShiftManager->AutoAlign(index, mItemEmpty[9] ? 0 : mItemInt[9], doShift, 
    flags, NULL, 
    mItemEmpty[6] ? 0.f : mItemFlt[6], mItemEmpty[7] ? 0.f : mItemFlt[7], (float)delX, 0.,
    0., NULL, NULL, true, NULL, NULL, mItemEmpty[8] ? 0.f : mItemFlt[8]);
  mDisableAlignTrim = false;
  if (index2 > 0)
    SUSPEND_NOLINE("because of failure to autoalign");
  SetReportedValues(index2);
  return 0;
}

// LimitNextAutoAlign
int CMacCmd::LimitNextAutoAlign(void)
{
  mShiftManager->SetNextAutoalignLimit(mItemFlt[1]);
  return 0;
}

// AlignWithRotation
int CMacCmd::AlignWithRotation(void)
{
  CString report;
  int index;
  float bmin, shiftX, shiftY;

  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report))
    ABORT_LINE(report);
  if (mItemDbl[3] < 0.2 || mItemDbl[3] > 50.)
    ABORT_LINE("The angle range to search must be between 0.2 and 50 degrees in:\n\n");
  if (mNavHelper->AlignWithRotation(index, mItemFlt[2], mItemFlt[3], bmin,
    shiftX, shiftY))
    ABORT_LINE("Failure to autoalign in:\n\n");
  SetRepValsAndVars(4, bmin, shiftX, shiftY);
  return 0;
}

// AutoFocus, G
int CMacCmd::AutoFocus(void)
{
  int index;

  index = 1;
  if (!mItemEmpty[1])
    index = mItemInt[1];
  if (index > -2 && !mFocusManager->FocusReady())
    SUSPEND_NOLINE("because autofocus not calibrated");
  if (mMinDeltaFocus != 0. || mMaxDeltaFocus != 0.)
    mFocusManager->NextFocusChangeLimits(mMinDeltaFocus, mMaxDeltaFocus);
  if (mMinAbsFocus != 0. || mMaxAbsFocus != 0.)
    mFocusManager->NextFocusAbsoluteLimits(mMinAbsFocus, mMaxAbsFocus);
  if (index < -1)
    mFocusManager->DetectFocus(FOCUS_REPORT,
    !mItemEmpty[2] ? mItemInt[2] : 0);
  else
    mFocusManager->AutoFocusStart(index,
      !mItemEmpty[2] ? mItemInt[2] : 0);
  mTestScale = true;
  return 0;
}

// BeamTiltDirection
int CMacCmd::BeamTiltDirection(void)
{
  mFocusManager->SetTiltDirection(mItemInt[1]);
  return 0;
}

// FocusChangeLimits
int CMacCmd::FocusChangeLimits(void)
{
  mMinDeltaFocus = mItemFlt[1];
  mMaxDeltaFocus = mItemFlt[2];
  return 0;
}

// AbsoluteFocusLimits
int CMacCmd::AbsoluteFocusLimits(void)
{
  mMinAbsFocus = mItemDbl[1];
  mMaxAbsFocus = mItemDbl[2];
  return 0;
}

// CorrectAstigmatism
int CMacCmd::CorrectAstigmatism(void)
{
  if (mWinApp->mAutoTuning->FixAstigmatism(mItemEmpty[1] || mItemInt[1] >= 0))
    ABORT_NOLINE("There is no astigmatism calibration for the current settings");
  return 0;
}

// CorrectComa
int CMacCmd::CorrectComa(void)
{
  int index;

  index = COMA_INITIAL_ITERS;
  if (mItemInt[1])
    index = mItemInt[1] > 0 ? COMA_ADD_ONE_ITER : COMA_JUST_MEASURE;
  if (mWinApp->mAutoTuning->ComaFreeAlignment(false, index))
    AbortMacro();
  return 0;
}

// ZemlinTableau
int CMacCmd::ZemlinTableau(void)
{
  int index, index2;

  index = 340;
  index2 = 170;
  if (mItemInt[2] > 10)
    index = mItemInt[2];
  if (mItemInt[3] > 0)
    index2 = mItemInt[3];
  mWinApp->mAutoTuning->MakeZemlinTableau(mItemFlt[1], index, index2);
  return 0;
}

// CBAstigComa
int CMacCmd::CBAstigComa(void)
{
  B3DCLAMP(mItemInt[1], 0, 2);
  if (mWinApp->mAutoTuning->CtfBasedAstigmatismComa(mItemInt[1], mItemInt[2] != 0,
    mItemInt[3], mItemInt[4] > 0, mItemInt[5] > 0)) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// FixAstigmatismByCTF, FixComaByCTF
int CMacCmd::FixAstigmatismByCTF(void)
{
  BOOL truth;
  int index;

  index = 0;
  if (CMD_IS(FIXCOMABYCTF)) {
     index = mWinApp->mAutoTuning->GetCtfDoFullArray() ? 2 : 1;
    if (!mItemEmpty[3])
      index = mItemInt[3] > 0 ? 2 : 1;
    truth = mItemInt[4] > 0;
  } else
    truth = mItemInt[3] > 0;
  if (mWinApp->mAutoTuning->CtfBasedAstigmatismComa(index, false,
    (mItemInt[1] > 0) ? 1 : 0, mItemInt[2] > 0, truth)){
    AbortMacro();
    return 1;
  }
  return 0;
}

// CalibrateComaVsIS
int CMacCmd::CalibrateComaVsIS(void)
{
  float extent = mWinApp->mAutoTuning->GetComaVsISextent();
  int rotation = mWinApp->mAutoTuning->GetComaVsISrotation();
  int full = mItemEmpty[3] ? mWinApp->mAutoTuning->GetComaVsISuseFullArray() :
    mItemInt[3];
  if (mItemFlt[1] > 0.)
    extent = mItemFlt[1];
  if (!mItemEmpty[2] && mItemInt[2] > -900)
    rotation = mItemInt[2];
  if (mWinApp->mAutoTuning->CalibrateComaVsImageShift(extent, rotation, full)) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// ReportStigmatorNeeded
int CMacCmd::ReportStigmatorNeeded(void)
{
  float backlashX, backlashY;

  mWinApp->mAutoTuning->GetLastAstigNeeded(backlashX, backlashY);
  mLogRpt.Format("Last measured stigmator change needed: %.4f  %.4f", backlashX,
    backlashY);
  SetRepValsAndVars(1, backlashX, backlashY);
  return 0;
}

// ReportComaTiltNeeded
int CMacCmd::ReportComaTiltNeeded(void)
{
  float backlashX, backlashY;

  mWinApp->mAutoTuning->GetLastBeamTiltNeeded(backlashX, backlashY);
  mLogRpt.Format("Last measured beam tilt change needed: %.2f  %.2f", backlashX,
    backlashY);
  SetRepValsAndVars(1, backlashX, backlashY);
  return 0;
}

// ReportComaVsISmatrix
int CMacCmd::ReportComaVsISmatrix(void)
{
  ComaVsISCalib *cvsis = mWinApp->mAutoTuning->GetComaVsIScal();
  if (cvsis->magInd <= 0)
    ABORT_LINE("There is no calibration of beam tilt versus image shift for line:\n\n");
  mLogRpt.Format("Coma versus IS calibration is %f  %f  %f  %f", cvsis->matrix.xpx,
    cvsis->matrix.xpy, cvsis->matrix.ypx, cvsis->matrix.ypy);
  SetRepValsAndVars(1, cvsis->matrix.xpx, cvsis->matrix.xpy,
    cvsis->matrix.ypx, cvsis->matrix.ypy);
  return 0;
}

// Save, S
int CMacCmd::Save(void)
{
  CString report;
  int index2, i;

  index2 = -1;
  if (ConvertBufferLetter(mStrItems[1], 0, true, i, report))
    ABORT_LINE(report);
  if (!mItemEmpty[2]) {
    index2 = mItemInt[2] - 1;
    if (index2 < 0 || index2 >= mWinApp->mDocWnd->GetNumStores())
      ABORT_LINE("File # to save to is beyond range of open file numbers in "
        "statement: \n\n");
  }

  report = i == 0 ? "A" : mStrItems[1];
  if (index2 < 0 && (mWinApp->Montaging() ||
    !mBufferManager->IsBufferSavable(mImBufs + i)))
    SUSPEND_NOLINE("because buffer " + report + " is not savable to current file");
  if (mItemEmpty[1])
    mWinApp->mDocWnd->SaveRegularBuffer();
  else if (mWinApp->mDocWnd->SaveBufferToFile(i, index2))
    SUSPEND_LINE("because of error saving to file in statement: \n\n");
  EMimageExtra *extra = (EMimageExtra * )mImBufs[i].mImage->GetUserData();
  if (extra) {
    if (index2 < 0)
      report.Format("Saved Z =%4d, %6.2f degrees", mImBufs[i].mSecNumber,
        extra->m_fTilt);
    else
      report.Format("Saved Z =%4d, %6.2f degrees to file #%d", mImBufs[i].mSecNumber,
        extra->m_fTilt, index2 + 1);
    mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
  }
  return 0;
}

// ReadFile, ReadMontagePiece
int CMacCmd::ReadFile(void)
{
  CString report;
  int index, index2, ix0, iy0;
  bool piece = CMD_IS(READMONTAGEPIECE);
  index = mItemInt[1];
  iy0 = mBufferManager->GetBufToReadInto();
  if (!mWinApp->mStoreMRC)
    SUSPEND_NOLINE("on " + CString(piece? "ReadMontagePiece" : "ReadFile") +
      " because there is no open file");
  if (ConvertBufferLetter(mStrItems[piece ? 4 : 2], iy0, false, ix0, report))
    ABORT_LINE(report);
  mBufferManager->SetBufToReadInto(ix0);
  if (mWinApp->Montaging() && mWinApp->mMontageController->GetRunningMacro())
    ABORT_LINE("Trying to read from a montage in script run from a montage in line:\n\n");
  if (piece) {
    if (!mWinApp->Montaging())
      ABORT_LINE("The current file is not a montage for line:\n\n")
    index = mBufferManager->FindSectionForPiece(mWinApp->mStoreMRC, *mMontP, index - 1,
      mItemInt[2] - 1, mItemInt[3]);
    if (index < 0)
      ABORT_LINE("Could not find piece in file for line:\n\n");
  }
  if (mWinApp->Montaging() && !piece)
    index2 = mWinApp->mMontageController->ReadMontage(index, NULL, NULL, false, true);
  else {
    index2 = mBufferManager->ReadFromFile(mWinApp->mStoreMRC, index);
    if (!index2)
      mWinApp->DrawReadInImage();
  }
  mBufferManager->SetBufToReadInto(iy0);
  if (index2)
    ABORT_NOLINE("Script stopped because of error reading from file");
  return 0;
}

// ReadOtherFile
int CMacCmd::ReadOtherFile(void)
{
  CString report;
  int index, index2, ix0, ix1, iy0;

  index = mItemInt[1];
  if (index < 0)
    ABORT_LINE("Section to read is negative in line:\n\n");
  iy0 = mBufferManager->GetBufToReadInto();
  if (mItemInt[2] < 0)
    ix0 = iy0;
  else if (ConvertBufferLetter(mStrItems[2], -1, false, ix0, report))
    ABORT_LINE(report);
  if (CheckConvertFilename(mStrItems, mStrLine, 3, report))
    return 1;
  for (ix1 = 0; ix1 <= mRetryReadOther; ix1++) {
    mBufferManager->SetOtherFile(report);
    mBufferManager->SetBufToReadInto(ix0);
    mBufferManager->SetNextSecToRead(index);
    index2 = mBufferManager->RereadOtherFile(mStrCopy);
    if (!index2) {
      mWinApp->DrawReadInImage();
      break;
    }
    if (ix1 < mRetryReadOther)
      Sleep(mReadOtherSleep);
  }
  mBufferManager->SetBufToReadInto(iy0);
  if (index2 && index2 != READ_MONTAGE_OK)
    ABORT_NOLINE("Script stopped because of error reading from other file:\n" +
    mStrCopy);
  return 0;
}

// RetryReadOtherFile
int CMacCmd::RetryReadOtherFile(void)
{
  mRetryReadOther = B3DMAX(0, mItemInt[1]);
  return 0;
}

// SaveToOtherFile, SnapshotToFile
int CMacCmd::SaveToOtherFile(void)
{
  CString report;
  BOOL truth;
  int index, index2, ix0, ix1, iy1;
  ScreenShotParams *snapParams = mWinApp->GetScreenShotParams();
  int compressions[] = {COMPRESS_NONE, COMPRESS_ZIP, COMPRESS_LZW, COMPRESS_JPEG};
  truth = CMD_IS(SNAPSHOTTOFILE);
  if (truth) {
    ix0 = 4;
    if (mItemDbl[1] > 0. && mItemDbl[1] < 1.)
      ABORT_LINE("Factor to zoom by must be >= 1 or <= 0 in line:\n\n");
    if (mItemDbl[2] > 0. && mItemDbl[2] < 1.)
      ABORT_LINE("Factor to scale sizes by must be >= 1 or <= 0 in line:\n\n");
  } else {
    ix0 = 2;
    if (ConvertBufferLetter(mStrItems[1], -1, true, index, report, true))
      ABORT_LINE(report);
  }
  index2 = -1;
  if (mStrItems[ix0] == "MRC")
    index2 = STORE_TYPE_MRC;
  else if (mStrItems[ix0] == "HDF")
    index2 = STORE_TYPE_HDF;
  else if (mStrItems[ix0] == "TIF" || mStrItems[2] == "TIFF")
    index2 = STORE_TYPE_TIFF;
  else if (mStrItems[ix0] == "JPG" || mStrItems[2] == "JPEG")
    index2 = STORE_TYPE_JPEG;
  else if (mStrItems[ix0] != "CUR" && mStrItems[2] != "-1")
    ABORT_LINE(CString(ix0 == 2 ? "Second" : "Fourth") + 
      " entry must be MRC, TIF, TIFF, JPG, JPEG, CUR, or -1 in line:\n\n");
  if (truth && (index2 == STORE_TYPE_MRC || index2 == STORE_TYPE_HDF))
    ABORT_LINE("A snapshot cannot be saved to an MRC or HDF file in line:\n\n");
  ix1 = -1;
  if (mStrItems[ix0 + 1] == "NONE")
    ix1 = COMPRESS_NONE;
  else if (mStrItems[ix0 + 1] == "LZW")
    ix1 = COMPRESS_LZW;
  else if (mStrItems[ix0 + 1] == "ZIP")
    ix1 = COMPRESS_ZIP;
  else if (mStrItems[ix0 + 1] == "JPG" || mStrItems[ix0 + 1] == "JPEG")
    ix1 = COMPRESS_JPEG;
  else if (mStrItems[ix0 + 1] != "CUR" && mStrItems[ix0 + 1] != "-1")
    ABORT_LINE(CString(ix0 == 2 ? "Third" : "Fifth") + 
      " entry must be NONE, LZW, ZIP, JPG, JPEG, CUR, or -1 in line:\n\n");
  if (CheckConvertFilename(mStrItems, mStrLine, ix0 + 2, report))
    return 1;
  if (truth) {
    if (index2 < 0)
      index2 = snapParams->fileType ? STORE_TYPE_JPEG : STORE_TYPE_TIFF;
    if (ix1 < 0) {
      B3DCLAMP(snapParams->compression, 0, 3);
      ix1 = compressions[snapParams->compression];
    }
    iy1 = mWinApp->mActiveView->TakeSnapshot(mItemFlt[1], mItemFlt[2],
      mItemInt[3], index2, ix1, snapParams->jpegQuality, report);
    if (CScreenShotDialog::GetSnapshotError(iy1, report)) {
      ABORT_LINE("Error taking snapshot, " + report + " for line:\n\n");
    }
  } else {
    iy1 = mWinApp->mDocWnd->SaveToOtherFile(index, index2, ix1, &report);
    if (iy1 == 1)
      return 1;
    if (iy1) {
      report.Format("Error %s file for line:\n\n", iy1 == 2 ? "opening" : "saving to");
      ABORT_LINE(report);
    }
  }
  return 0;
}

// OpenNewFile, OpenNewMontage, OpenFrameSumFile
int CMacCmd::OpenNewFile(void)
{
  CString report;
  int index, index2, ix0, iy0, setNum, xFrame;
  CFileStatus status;
  if (mWinApp->mMontageController->DoingMontage() &&
    mWinApp->mMontageController->GetRunningMacro())
    ABORT_LINE("Trying to do file operation in script run from montage in line:\n\n");

  index = 1;
  if (CMD_IS(OPENNEWMONTAGE)) {
    index = 3;
    ix0 = mItemInt[1];
    if (!ix0)
      ix0 = mMontP->xNframes;
    iy0 = mItemInt[2];
    if (!iy0)
      iy0 = mMontP->yNframes;
    if (ix0 < 1 || iy0 < 1)
      ABORT_LINE("Number of montages pieces is missing or not positive"
        " in statement:\n\n");
  }
  if (CMD_IS(OPENFRAMESUMFILE)) {
    SubstituteLineStripItems(mStrLine, index, mStrCopy);
    report = mCamera->GetFrameFilename() + mStrCopy;
    mWinApp->AppendToLog("Opening file: " + report, LOG_SWALLOW_IF_CLOSED);
  } else if (CheckConvertFilename(mStrItems, mStrLine, index, report))
    return 1;

  // Check if file already exists
  if (!mOverwriteOK && CFile::GetStatus((LPCTSTR)report, status))
    SUSPEND_NOLINE("opening new file because " + report + " already exists");

  if (index == 1)
    index2 = mWinApp->mDocWnd->DoOpenNewFile(report);
  else {
    mWinApp->mDocWnd->LeaveCurrentFile();
    mMontP->xNframes = ix0;
    mMontP->yNframes = iy0;
    setNum = MontageConSetNum(mMontP, false);
    xFrame = mMontP->xFrame;

    // If the binning has changed, start from scratch on frame sizes and overlaps
    if (mConSets[setNum].binning != mMontP->binning)
      xFrame = 0;
    index2 = mWinApp->mDocWnd->GetMontageParamsAndFile(xFrame > 0, ix0, iy0,
      report);
  }
  if (index2)
    SUSPEND_LINE("because of error opening new file in statement:\n\n");
  mWinApp->mBufferWindow.UpdateSaveCopy();
  return 0;
}

// SetupWaffleMontage
int CMacCmd::SetupWaffleMontage(void)
{
  CString report;
  int index2, ix0, iy0, sizeX, sizeY;
  float backlashX;

  if (mItemInt[1] < 2)
    ABORT_LINE("Minimum number of blocks in waffle grating must be at least 2 in:\n\n");
  backlashX = (float)(0.462 * mItemInt[1]);
  sizeX = mConSets[RECORD_CONSET].right - mConSets[RECORD_CONSET].left;
  sizeY = mConSets[RECORD_CONSET].bottom - mConSets[RECORD_CONSET].top;
  ix0 = PiecesForMinimumSize(backlashX, sizeX, 0.1f);
  iy0 = PiecesForMinimumSize(backlashX, sizeY, 0.1f);
  if (ix0 < 2 && iy0 < 2) {
    mWinApp->AppendToLog("No montage is needed to measure pixel size at this "
      "magnification");
    SetReportedValues(0.);
  } else {
    if (mWinApp->Montaging() && mMontP->xNframes == ix0 && mMontP->yNframes == iy0) {
      mMontP->magIndex = mScope->GetMagIndex();
      mWinApp->AppendToLog("Existing montage can be used to measure pixel size at "
        "this magnification");
    } else {

                 // If it is montaging already, close file
      if (mWinApp->Montaging())
        mWinApp->mDocWnd->DoCloseFile();

                 // Follow same procedure as above for opening montage file
      if (CheckConvertFilename(mStrItems, mStrLine, 2, report))
        return 1;
      mWinApp->mDocWnd->LeaveCurrentFile();
      mMontP->xNframes = ix0;
      mMontP->yNframes = iy0;
      mMontP->binning = mConSets[RECORD_CONSET].binning;
      mMontP->xFrame = sizeX / mMontP->binning;
      mMontP->yFrame = sizeY / mMontP->binning;
      mMontP->xOverlap = mMontP->xFrame / 10;
      mMontP->yOverlap = mMontP->yFrame / 10;
      index2 = mWinApp->mDocWnd->GetMontageParamsAndFile(true, ix0, iy0, report);
      mWinApp->mBufferWindow.UpdateSaveCopy();
      if (index2)
        ABORT_NOLINE("Error trying to open new montage the right size for current"
        " magnification");
      PrintfToLog("Opened file %s for %d x %d montage", (LPCTSTR)report, ix0, iy0);
    }
    mMontP->warnedMagChange = true;
    mMontP->overviewBinning = 1;
    mMontP->shiftInOverview = true;
    mWinApp->mMontageWindow.UpdateSettings();
    SetReportedValues(1.);
  }
  return 0;
}

// SetupFullMontage
int CMacCmd::SetupFullMontage(void)
{
  CFileStatus status;
  ABORT_NONAV;
  if (mItemFlt[1] < 0 || mItemFlt[1] >= 0.5)
    ABORT_LINE("Overlap factor must be between 0 (for unspecified) and 0.5 in line:\n\n");
  SubstituteLineStripItems(mStrLine, 2, mEnteredName);

  // Check if file already exists
  if (!mOverwriteOK && CFile::GetStatus((LPCTSTR)mEnteredName, status))
    SUSPEND_NOLINE("opening setting up a full montage because " + mEnteredName +
      " already exists");
  mNavigator->FullMontage(true, mItemFlt[1], true);
  return 0;
}

// SetupPolygonMontage
int CMacCmd::SetupPolygonMontage(void)
{
  CFileStatus status;
  int index;
  CMapDrawItem *navItem;
  CMontageSetupDlg dlg;

  index = mItemEmpty[1] ? 0 : mItemInt[1];
  navItem = CurrentOrIndexedNavItem(index, mStrLine);
  if (!navItem)
    return 1;
  if (navItem->IsNotPolygon())
    ABORT_LINE("Navigator item is not a polygon for line:\n\n");
  if (mItemFlt[2] < 0 || mItemFlt[2] >= 0.5)
    ABORT_LINE("Overlap factor must be between 0 (for unspecified) and 0.5 in line:\n\n");
  SubstituteLineStripItems(mStrLine, 3, mEnteredName);
  if (!mOverwriteOK && CFile::GetStatus((LPCTSTR)mEnteredName, status))
    SUSPEND_NOLINE("opening setting up a polygon  montage because " + mEnteredName +
      " already exists");
  if (mNavigator->PolygonMontage(NULL, true, index, mItemFlt[2], true))
    ABORT_LINE("An error occurred setting up a polygon montage for line:\n\n");
  return 0;
}

// SetGridMapLimits
int CMacCmd::SetGridMapLimits(void)
{
  float *gridLim = mNavHelper->GetGridLimits();
  int ind;
  if (mItemFlt[2] > 0 || mItemFlt[3] < 0 || mItemFlt[4] > 0 || mItemFlt[5] < 0)
    ABORT_LINE("Lower limits must be negative and upper limits must be positive for "
      "line:\n\n");
  if (!mItemInt[1]) {
    mRestoreGridLimits = true;
    mNumStatesToRestore++;
    for (ind = 0; ind < 4; ind++)
      mGridLimitsToRestore[ind] = gridLim[ind];
  }
  for (ind = 0; ind < 4; ind++) {
    if (mItemFlt[ind + 2] * ((ind % 2) ? 1 : -1) > 0) {
      if (ind % 2)
        ACCUM_MIN(mItemFlt[ind + 2], mScope->GetStageLimit(ind));
      else
        ACCUM_MAX(mItemFlt[ind + 2], mScope->GetStageLimit(ind));
      gridLim[ind] = mItemFlt[ind + 2];
    }
  }
  return 0;
}

// ReportNumMontagePieces
int CMacCmd::ReportNumMontagePieces(void)
{
  int index;

  if (mWinApp->Montaging()) {
    index = mMontP->xNframes * mMontP->yNframes -
      (mMontP->ignoreSkipList ? 0 : mMontP->numToSkip);
    mLogRpt.Format("%d pieces will be acquired in the current montage", index);
  } else {
    index = 1;
    mLogRpt = "Montaging is not used for the current file";
  }
  SetRepValsAndVars(1, index);
  return 0;
}

// EnterNameOpenFile
int CMacCmd::EnterNameOpenFile(void)
{
  mStrCopy = "Enter name for new file:";
  if (!mStrItems[1].IsEmpty())
    JustStripItems(mStrLine, 1, mStrCopy);
  if (!KGetOneString(mStrCopy, mEnteredName, 100))
    SUSPEND_NOLINE("because no new file was opened");
  if (mWinApp->mDocWnd->DoOpenNewFile(mEnteredName))
    SUSPEND_NOLINE("because of error opening new file by that name");
  return 0;
}

// ChooserForNewFile
int CMacCmd::ChooserForNewFile(void)
{
  CString report;
  int index;

  index = (mItemInt[1] != 0) ? STORE_TYPE_ADOC : STORE_TYPE_MRC;
  if (mWinApp->mDocWnd->FilenameForSaveFile(mItemInt[1], NULL, mStrCopy)) {
    AbortMacro();
    return 1;
  }
  report = mStrItems[2];
  report.MakeUpper();
  if (SetVariable(report, mStrCopy, VARTYPE_REGULAR, -1, false))
    ABORT_NOLINE("Error setting variable " + mStrItems[2] + " with filename " + mStrCopy);
  mOverwriteOK = true;
  return 0;
}

// ReadTextFile, Read2DTextFile, ReadStringsFromFile
int CMacCmd::ReadTextFile(void)
{
  CString report;
  BOOL truth;
  int index, index2;

  SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  CString newVar;
  CStdioFile *csFile = NULL;
  CArray < ArrayRow, ArrayRow > *rowsFor2d = NULL;
  ArrayRow arrRow;
  truth = CMD_IS(READ2DTEXTFILE);
  try {
    csFile = new CStdioFile(mStrCopy, CFile::modeRead | CFile::shareDenyWrite);
  }
  catch (CFileException * perr) {
    perr->Delete();
    ABORT_NOLINE("Error opening file " + mStrCopy);
  }
  if (truth)
    rowsFor2d = new CArray < ArrayRow, ArrayRow > ;
  while ((index = mParamIO->ReadAndParse(csFile, report, mStrItems,
    MAX_MACRO_TOKENS)) == 0) {
    if (truth)
      newVar = "";
    if (CMD_IS(READSTRINGSFROMFILE)) {
      if (newVar.GetLength())
        newVar += '\n';
      newVar += report;
    } else {
      for (index2 = 0; index2 < MAX_MACRO_TOKENS; index2++) {
        if (mStrItems[index2].IsEmpty())
          break;
        if (newVar.GetLength())
          newVar += '\n';
        newVar += mStrItems[index2];
      }
    }
    if (truth && index2 > 0) {
      arrRow.value = newVar;
      arrRow.numElements = index2;
      rowsFor2d->Add(arrRow);
    }
  }
  delete csFile;
  if (index > 0) {
    delete rowsFor2d;
    ABORT_NOLINE("Error reading text from file " + mStrCopy);
  }

  if (SetVariable(mItem1upper, truth ? "0" : newVar, VARTYPE_REGULAR, -1, false,
    &report, rowsFor2d)) {
    delete rowsFor2d;
    ABORT_NOLINE("Error setting an array variable with text from file " + mStrCopy +
      ":\n" + report);
  }
  return 0;
}

// OpenTextFile
int CMacCmd::OpenTextFile(void)
{
  CString report;
  BOOL truth;
  int index;
  CFileStatus status;
  FileForText *txFile;
  UINT openFlags = CFile::typeText;
  if (LookupFileForText(mItem1upper, TXFILE_MUST_NOT_EXIST, mStrLine, index))
    return 1;
  report = mStrItems[2].MakeUpper();
  if (report.GetLength() > 1 || CString("RTWAO").Find(report) < 0)
    ABORT_LINE("The second entry must be R, T, W, A, or O in line\n\n:");
  SubstituteLineStripItems(mStrLine, 4, mStrCopy);

  // Set up flags and read/write from mode entry
  if (report == "R" || report == "T") {
    openFlags |= CFile::shareDenyNone | CFile::modeRead;
    truth = true;
  } else {
    truth = false;
    openFlags |= CFile::shareDenyWrite;
    if (report == "A") {
      openFlags |= CFile::modeReadWrite;
    } else {
      openFlags |= CFile::modeWrite | CFile::modeCreate;
      if (report == "W" && !mOverwriteOK && CFile::GetStatus((LPCTSTR)mStrCopy, status))
        ABORT_LINE("File already exists and you entered W instead of O for line:\n\n");
    }
  }

  // Create new entry and try to open file; allowing  failure with 'T'
  txFile = new FileForText;
  txFile->readOnly = truth;
  txFile->ID = mItem1upper;
  txFile->persistent = mItemInt[3] != 0;
  txFile->csFile = NULL;
  try {
    txFile->csFile = new CStdioFile(mStrCopy, openFlags);
    if (report == "A")
      txFile->csFile->SeekToEnd();
  }
  catch (CFileException * perr) {
    perr->Delete();
    delete txFile->csFile;
    delete txFile;
    if (report != "T")
      ABORT_NOLINE("Error opening file " + mStrCopy);
    txFile = NULL;
  }

  // Add to array if success; set value for 'T'
  if (txFile)
    mTextFileArray.Add(txFile);
  if (report == "T")
    SetReportedValues(txFile ? 1 : 0);
  return 0;
}

// WriteLineToFile
int CMacCmd::WriteLineToFile(void)
{
  int index;
  FileForText *txFile;

  txFile = LookupFileForText(mItem1upper, TXFILE_WRITE_ONLY, mStrLine, index);
  if (!txFile)
    return 1;

  // To allow comments to be written, there is only one required argument in initial
  // check so we need to check here
  SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  try {
    txFile->csFile->WriteString(mStrCopy + "\n");
  }
  catch (CFileException * perr) {
    perr->Delete();
    ABORT_LINE("Error writing string to file for line:\n\n");
  }
  return 0;
}

// ReadLineToArray, ReadLineToString
int CMacCmd::ReadLineToArray(void)
{
  CString report;
  BOOL truth;
  int index, index2;
  FileForText *txFile;

  txFile = LookupFileForText(mItem1upper, TXFILE_READ_ONLY, mStrLine, index);
  if (!txFile)
    return 1;
  mStrLine = mStrItems[2];
  truth = CMD_IS(READLINETOARRAY);

  // Skip blank lines, skip comment lines if reading into array
  for (;;) {
    index = mParamIO->ReadAndParse(txFile->csFile, report, mStrItems,
      MAX_MACRO_TOKENS, mParseQuotes);
    if (index || !mStrItems[0].IsEmpty() || (!truth && !report.IsEmpty()))
      break;
  }

  // Check error conditions
  if (index > 0)
    ABORT_LINE("Error reading from file for line:\n\n");
  if (index) {
    mWinApp->AppendToLog("End of file reached for file with ID " + txFile->ID,
      mLogAction);
  } else {

    // For array, concatenate items into report; otherwise report is all set
    if (truth) {
      report = "";
      for (index2 = 0; index2 < MAX_MACRO_TOKENS; index2++) {
        if (mStrItems[index2].IsEmpty())
          break;
        if (report.GetLength())
          report += '\n';
        report += mStrItems[index2];
      }
    }
    if (SetVariable(mStrLine, report, VARTYPE_REGULAR, -1, false, &mStrCopy))
      ABORT_NOLINE("Error setting a variable with line from file:\n" + mStrCopy);
  }
  SetReportedValues(index ? 0 : 1);
  return 0;
}

// CloseTextFile
int CMacCmd::CloseTextFile(void)
{
  int index;

  if (!LookupFileForText(mItem1upper, TXFILE_MUST_EXIST, mStrLine, index))
    return 1;
  CloseFileForText(index);
  return 0;
}

// FlushTextFile
int CMacCmd::FlushTextFile(void)
{
  int index;
  FileForText *txFile;

  txFile = LookupFileForText(mItem1upper, TXFILE_MUST_EXIST, mStrLine, index);
  if (!txFile)
    return 1;
  txFile->csFile->Flush();
  return 0;
}

// IsTextFileOpen
int CMacCmd::IsTextFileOpen(void)
{
  BOOL truth;
  int index;
  FileForText *txFile;

  txFile = LookupFileForText(mItem1upper, TXFILE_QUERY_ONLY, mStrLine, index);
  truth = false;
  if (!txFile && !mItemEmpty[2]) {

    // Check the file name if the it doesn't match ID
    char fullBuf[_MAX_PATH];
    SubstituteLineStripItems(mStrLine, 2, mStrCopy);
    if (GetFullPathName((LPCTSTR)mStrCopy, _MAX_PATH, fullBuf, NULL) > 0) {
      for (index = 0; index < mTextFileArray.GetSize(); index++) {
        txFile = mTextFileArray.GetAt(index);
        if (!(txFile->csFile->GetFilePath()).CompareNoCase(fullBuf)) {
          truth = true;
          break;
        }
      }
    }
    mLogRpt.Format("Text file with name %s %s open", (LPCTSTR)mStrCopy, truth ? "IS" :
      "is NOT");
    SetReportedValues(truth ? 1 : 0);
  } else {
    mLogRpt.Format("Text file with identifier %s %s open", (LPCTSTR)mItem1upper,
      txFile ? "IS" : "is NOT");
    SetReportedValues(txFile ? 1 : 0);
  }
  return 0;
}

// UserSetDirectory
int CMacCmd::UserSetDirectory(void)
{
  mStrCopy = "Choose a new current working directory:";
  if (!mStrItems[1].IsEmpty())
    JustStripItems(mStrLine, 1, mStrCopy);
  char *cwd = _getcwd(NULL, _MAX_PATH);
  CXFolderDialog dlg(cwd);
  dlg.SetTitle(mStrCopy);
  free(cwd);
  if (dlg.DoModal() == IDOK) {
    if (_chdir((LPCTSTR)dlg.GetPath()))
      SUSPEND_NOLINE("because of failure to change directory to " + dlg.GetPath());
  }
  SetOneReportedValue(dlg.GetPath(), 1);
  return 0;
}

// OpenChooserInCurrentDir
int CMacCmd::OpenChooserInCurrentDir(void)
{
  mWinApp->mDocWnd->SetInitialDirToCurrentDir();
  return 0;
}

// SetNewFileType
int CMacCmd::SetNewFileType(void)
{
  FileOptions *fileOpt = mWinApp->mDocWnd->GetFileOpt();
  if (mItemInt[2] != 0)
    fileOpt->montFileType = mItemInt[1] != 0 ? STORE_TYPE_ADOC : STORE_TYPE_MRC;
  else
    fileOpt->fileType = mItemInt[1] != 0 ? STORE_TYPE_ADOC : STORE_TYPE_MRC;
  return 0;
}

// OpenOldFile
int CMacCmd::OpenOldFile(void)
{
  CString report;
  CFile *cfile;
  int index;

  if (mWinApp->mMontageController->DoingMontage() &&
    mWinApp->mMontageController->GetRunningMacro())
    ABORT_LINE("Trying to do file operation in script run from montage in line:\n\n");
  if (CheckConvertFilename(mStrItems, mStrLine, 1, report))
    return 1;
  index = mWinApp->mDocWnd->OpenOldMrcCFile(&cfile, report, false);
  if (index == MRC_OPEN_NOERR || index == MRC_OPEN_ADOC || index == MRC_OPEN_HDF)
    index = mWinApp->mDocWnd->OpenOldFile(cfile, report, index, true);
  if (index != MRC_OPEN_NOERR)
    SUSPEND_LINE("because of error opening old file in statement:\n\n");
  return 0;
}

int CMacCmd::UserOpenOldFile(void)
{
  if (mWinApp->mMontageController->DoingMontage() &&
    mWinApp->mMontageController->GetRunningMacro())
    ABORT_LINE("Trying to do file operation in script run from montage in line:\n\n");
  if (mWinApp->mDocWnd->DoFileOpenold())
    SUSPEND_LINE("because existing image file was not opened in line:\n\n");
  return 0;
}

// CloseFile
int CMacCmd::CloseFile(void)
{
  if (mWinApp->mMontageController->DoingMontage() &&
    mWinApp->mMontageController->GetRunningMacro())
    ABORT_LINE("Trying to do file operation in script run from montage in line:\n\n");
  if (mWinApp->mStoreMRC) {
    mWinApp->mDocWnd->DoCloseFile();
  } else if (!mItemEmpty[1]) {
    SUSPEND_LINE("Script suspended on CloseFile because there is no open file");
  }
  return 0;
}

// RemoveFile
int CMacCmd::RemoveFile(void)
{
  CString report;

  if (CheckConvertFilename(mStrItems, mStrLine, 1, report))
    return 1;
  try {
    CFile::Remove(report);
  }
  catch (CFileException * pEx) {
    pEx->Delete();
    PrintfToLog("WARNING: File %s cannot be removed", (LPCTSTR)report);
  }
  return 0;
}

// ReportCurrentFilename, ReportLastFrameFile, ReportNavFile
int CMacCmd::ReportCurrentFilename(void)
{
  CString report;
  BOOL truth;

  truth = !CMD_IS(REPORTLASTFRAMEFILE);
  if (CMD_IS(REPORTCURRENTFILENAME)) {
    if (!mWinApp->mStoreMRC)
      SUSPEND_LINE("because there is no file open currently for statement:\n\n");
    report = mWinApp->mStoreMRC->getFilePath();
    mStrCopy = "Current open image file is: ";
  }
  else if (truth) {
    ABORT_NONAV;
    report = mWinApp->mNavigator->GetCurrentNavFile();
    if (report.IsEmpty())
      ABORT_LINE("There is no Navigator file open for:\n\n");
    mStrCopy = "Current open Navigator file is: ";
  }
  else {
    report = mCamera->GetPathForFrames();
    if (report.IsEmpty())
      ABORT_LINE("There is no last frame file name available for:\n\n");
    mStrCopy = "Last frame file is: ";
  }
  mWinApp->AppendToLog(mStrCopy + report, mLogAction);
  CString root = report;
  CString ext;
  if (mItemInt[1] && truth)
    UtilSplitExtension(report, root, ext);
  SetOneReportedValue(!truth ? &mStrItems[1] : NULL, root, 1);

  if (!ext.IsEmpty())
    SetOneReportedValue(ext, 2);
  if (mItemInt[1] && truth) {
    UtilSplitPath(root, report, ext);
    SetOneReportedValue(report, 3);
    SetOneReportedValue(ext, 4);
  }
  if (!truth) {
    UtilSplitPath(root, report, ext);
    SetOneReportedValue(&mStrItems[1], report, 2);
    SetOneReportedValue(&mStrItems[1], ext, 3);
  }
  return 0;
}

// ReportFrameBaseName
int CMacCmd::ReportFrameBaseName(void)
{
  int index;

  mStrCopy = mCamera->GetFrameBaseName();
  index = mCamera->GetFrameNameFormat();
  mLogRpt.Format("The frame base name %s used in frame names, %s used in folder "
    "names, and is %s", (index & FRAME_FILE_ROOT) ? "IS" : "IS NOT",
    (index & FRAME_FOLDER_ROOT) ? "IS" : "IS NOT",
    mStrCopy.IsEmpty() ? "not defined" : (LPCTSTR)mStrCopy);
  if (mStrCopy.IsEmpty())
    mStrCopy = "none";
  SetOneReportedValue(&mStrItems[1], (index & FRAME_FILE_ROOT) ? 1. : 0., 1);
  SetOneReportedValue(&mStrItems[1], mStrCopy, 2);
  SetOneReportedValue(&mStrItems[1], (index & FRAME_FOLDER_ROOT) ? 1. : 0., 3);
  return 0;
}

// SetFrameBaseName
int CMacCmd::SetFrameBaseName(void)
{
  unsigned int format = (unsigned int)mCamera->GetFrameNameFormat();;

  if (!mItemInt[1]) {
    mSavedFrameBaseName = mCamera->GetFrameBaseName();
    mSavedFrameNameFormat = format;
    mNumStatesToRestore++;
  }
  if (mItemInt[2] >= 0)
    setOrClearFlags(&format, FRAME_FILE_ROOT, mItemInt[2]);
  if (mItemInt[3] >= 0)
    setOrClearFlags(&format, FRAME_FOLDER_ROOT, mItemInt[3]);
  mCamera->SetFrameNameFormat(format);
  SubstituteLineStripItems(mStrLine, 4, mStrCopy);
  mCamera->SetFrameBaseName(mStrCopy);
  return 0;
}

// ReportFrameSavingPath
int CMacCmd::ReportFrameSavingPath(void)
{
  if (!(mCamParams->K2Type || mCamParams->canTakeFrames) || mCamParams->FEItype)
    ABORT_NOLINE("ReportFrameSavingPath works only with Gatan and generic frame-saving"
      " cameras");
  if (mCamParams->K2Type)
    mStrCopy = mCamera->GetDirForK2Frames();
  else
    mStrCopy = mCamParams->dirForFrameSaving;
  if (mStrCopy.IsEmpty()) {
    mLogRpt = "No frame-saving path is defined for " + mCamParams->name;
    mStrCopy = "NONE";
  } else
    mLogRpt = "The frames saving path for " + mCamParams->name + " is " + mStrCopy;

  SetOneReportedValue(&mStrItems[1], mStrCopy, 1);
  return 0;
}

// GetFileInWatchedDir, RunScriptInWatchedDir, RunScriptInFile, RunSerialEMSnapshot
int CMacCmd::GetFileInWatchedDir(void)
{
  CString report;
  BOOL truth;
  bool doSnapshot = CMD_IS(RUNSERIALEMSNAPSHOT);
  bool justFromFile = CMD_IS(RUNSCRIPTINFILE) || doSnapshot;
  int index;
  CFileStatus status;
  WIN32_FIND_DATA findFileData;
  CStdioFile *sFile = NULL;
  CString direc, fname;
  if (CMD_IS(RUNSCRIPTINWATCHEDDIR) || justFromFile) {
    if (mNumTempMacros >= MAX_TEMP_MACROS)
      ABORT_LINE("No free temporary scripts available for line:\n\n");
    if (mNeedClearTempMacro >= 0)
      ABORT_LINE("When running a script from a file, you cannot run another one in"
        " line:\n\n");
  }
  if (doSnapshot) {
    report = mWinApp->mPluginManager->GetExePath() + "\\SerialEM_Snapshot.txt";
    if (!CFile::GetStatus((LPCTSTR)report, status)) {
      report = "C:\\Program Files\\SerialEM\\SerialEM_Snapshot.txt";
      if (!CFile::GetStatus((LPCTSTR)report, status))
        ABORT_LINE("Cannot find SerialEM_Snapshot.txt in SerialEM program directory for "
          "line:\n\n");
    }
  } else if (CheckConvertFilename(mStrItems, mStrLine, 1, report))
    return 1;

  if (!justFromFile) {

    // If the string has no wild card and it is a directory, add *
    if (report.Find('*') < 0) {
      truth = CFile::GetStatus((LPCTSTR)report, status);
      if (truth && (status.m_attribute & CFile::directory))
        report += "\\*";
    }
    UtilSplitPath(report, direc, fname);
    HANDLE hFind = FindFirstFile((LPCTSTR)report, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
      SetOneReportedValue(0., 1);
      SetOneReportedValue(CString("none"), 2);
      return 0;
    }
    truth = false;
    do {
      report = findFileData.cFileName;

      // If found a file, look for lock file and wait until goes away, or give up and
      // go on to next file if any
      for (index = 0; index < 10; index++) {
        if (!CFile::GetStatus((LPCTSTR)(report + ".lock"), status)) {
          truth = true;
          break;
        }
        Sleep(20);
      }
      if (truth)
        break;

    } while (FindNextFile(hFind, &findFileData) == 0);
    FindClose(hFind);

    // Just set report value if no file. give message if there is one
    if (!truth) {
      SetOneReportedValue(0., 1);
      SetOneReportedValue(CString("none"), 2);
    } else {
      report = direc + "\\" + report;
      SetOneReportedValue(1., 1);
      SetOneReportedValue(report, 2);
      mLogRpt = "Found file " + report;
    }
  }

  if (CMD_IS(GETFILEINWATCHEDDIR))
    return 0;

  index = RunScriptFromFile(report, !justFromFile, mStrCopy);
  if (index > 0) {
    if (mStrCopy.IsEmpty())
      return 1;
    ABORT_LINE(mStrCopy + " for line:\n\n");
  }
  mLogRpt = "Running script in " + report;
  if (index < 0)
    mLogRpt += ", which is empty (nothing to run)";
  return 0;
}

// AllowFileOverwrite
int CMacCmd::AllowFileOverwrite(void)
{
  mOverwriteOK = mItemInt[1] != 0;
  return 0;
}

// SetDirectory, MakeDirectory
int CMacCmd::SetDirectory(void)
{
  CFileStatus status;
  CString mess;

  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  if (mStrCopy.IsEmpty())
    ABORT_LINE("Missing directory name in statement:\n\n");
  if (CMD_IS(SETDIRECTORY)) {
    if (_chdir((LPCTSTR)mStrCopy))
      SUSPEND_NOLINE("because of failure to change directory to " + mStrCopy);
  } else {
    if (CFile::GetStatus((LPCTSTR)mStrCopy, status)) {
      mWinApp->AppendToLog("Not making directory " + mStrCopy + " - it already exists");
    } else {
      if (!UtilRecursiveMakeDir(mStrCopy, mess))
        mWinApp->AppendToLog("Created directory " + mStrCopy);
      else
        SUSPEND_NOLINE("because of failure to create directory " + mStrCopy + ":\n" + 
          mess);
    }
  }
  return 0;
}

// ReportDirectory
int CMacCmd::ReportDirectory(void)
{
  char *cwd = _getcwd(NULL, _MAX_PATH);
  if (!cwd)
    ABORT_LINE("Could not determine current directory for:\n\n");
  mStrCopy = cwd;
  free(cwd);
  mWinApp->AppendToLog("Current directory is " + mStrCopy, mLogAction);
  SetOneReportedValue(&mStrItems[1], mStrCopy, 1);
  return 0;
}

// DoesFileExist
int CMacCmd::DoesFileExist(void)
{
  CString report;
  int index;
  CFileStatus status;

  if (CheckConvertFilename(mStrItems, mStrLine, 1, report))
    return 1;
  index = CFile::GetStatus((LPCTSTR)report, status) ? 1 : 0;
  mLogRpt.Format("File %s DOES%s exist", report, index ? "" : " NOT");
  SetReportedValues(index);
  return 0;
}

// MakeDateTimeDir
int CMacCmd::MakeDateTimeDir(void)
{
  mStrCopy = mWinApp->mDocWnd->DateTimeForFrameSaving();
  if (_mkdir((LPCTSTR)mStrCopy))
    SUSPEND_NOLINE("because of failure to create directory " + mStrCopy);
  if (_chdir((LPCTSTR)mStrCopy))
    SUSPEND_NOLINE("because of failure to change directory to " + mStrCopy);
  mWinApp->AppendToLog("Created directory " + mStrCopy);
  return 0;
}

// SwitchToFile
int CMacCmd::SwitchToFile(void)
{
  int index;

  index = mItemInt[1];
  if (index < 1 || index > mWinApp->mDocWnd->GetNumStores())
    ABORT_LINE("Number of file to switch to is absent or out of range in statement:"
      " \n\n");
  mWinApp->mDocWnd->SetCurrentStore(index - 1);
  return 0;
}

// ReportFileNumber
int CMacCmd::ReportFileNumber(void)
{
  int index;

  index = mWinApp->mDocWnd->GetCurrentStore();
  if (index >= 0) {
    index++;
    mLogRpt.Format("Current open file number is %d", index);
  } else
    mLogRpt = "There is no file open currently";
  SetRepValsAndVars(1, index);
  return 0;
}

// AddTitleToFile
int CMacCmd::AddTitleToFile(void)
{
  KImageStore *store = mWinApp->mStoreMRC;
  int index = mItemInt[1];
  if (!index) {
    if (!store)
      ABORT_LINE("There is no open image file for:\n\n");
  } else {
    if (index < 0 || index > mWinApp->mDocWnd->GetNumStores())
      ABORT_LINE("File number is out of range in:\n\n");
    store = mWinApp->mDocWnd->GetStoreMRC(index - 1);
  }
  if (!store->getDepth())
    ABORT_LINE("at least one image must be saved before running:\n\n");
  SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  index = store->AddTitle(mStrCopy);
  if (index)
    PrintfToLog("Error %d adding title to autodoc structure", index);
  index = store->WriteHeader(true);
  if (index > 0)
    mLogRpt = "Error rewriting the file header with new title";
  else if (index < 0)
    mLogRpt.Format("Error %d rewriting the mdoc file with new title", index);
  return 0;
}

// AddToAutodoc, WriteAutodoc
int CMacCmd::AddToAutodoc(void)
{
  int index, index2;

  if (!mWinApp->mStoreMRC)
    ABORT_LINE("There is no open image file for: \n\n");
  if (mBufferManager->CheckAsyncSaving())
    ABORT_NOLINE("There was an error writing to file.\n"
    "Not adding to the autodoc because it would go into the wrong section");
  index = mWinApp->mStoreMRC->GetAdocIndex();
  index2 = mWinApp->mStoreMRC->getDepth();
  if (index < 0)
    ABORT_LINE("There is no .mdoc file for the current image file for: \n\n");
  if (AdocGetMutexSetCurrent(index) < 0)
    ABORT_LINE("Error making autodoc be the current one for: \n\n");
  if (CMD_IS(ADDTOAUTODOC)) {
    SubstituteLineStripItems(mStrLine, 2, mStrCopy);
    if (!mRunningScrpLang)
      mParamIO->ParseString(mStrLine, mStrItems, MAX_MACRO_TOKENS, mParseQuotes);
    if (AdocSetKeyValue(
      index2 ? B3DCHOICE(mWinApp->mStoreMRC->getStoreType() == STORE_TYPE_ADOC,
      ADOC_IMAGE, ADOC_ZVALUE) : ADOC_GLOBAL,
      index2 ? index2 - 1 : 0, (LPCTSTR)mStrItems[1], (LPCTSTR)mStrCopy)) {
        AdocReleaseMutex();
        ABORT_LINE("Error adding string to autodoc file for: \n\n");
    }
  } else if (AdocWrite((char * )(LPCTSTR)mWinApp->mStoreMRC->getAdocName()) < 0) {
    AdocReleaseMutex();
    ABORT_NOLINE("Error writing to autodoc file");
  }
  AdocReleaseMutex();
  return 0;
}

// AddToFrameMdoc, WriteFrameMdoc
int CMacCmd::AddToFrameMdoc(void)
{
  int index;
  const char * frameMsg[] = { "There is no autodoc for frames open in:\n\n",
    "Error selecting frame .mdoc as current autodoc in:\n\n",
    "There is no current section to add to in frame .mdoc for:\n\n",
    "Error adding to frame .mdoc in:\n\n",
    "Error writing to frame .mdoc in:\n\n" };
  if (CMD_IS(ADDTOFRAMEMDOC)) {
    SubstituteLineStripItems(mStrLine, 2, mStrCopy);
    if (!mRunningScrpLang)
      mParamIO->ParseString(mStrLine, mStrItems, MAX_MACRO_TOKENS, mParseQuotes);
    index = mWinApp->mDocWnd->AddValueToFrameMdoc(mStrItems[1], mStrCopy);
  }
  else {
    index = mWinApp->mDocWnd->WriteFrameMdoc();
  }
  if (index > 0)
    ABORT_LINE(CString(frameMsg[index - 1]));
  return 0;
}

// ReportFrameMdocOpen
int CMacCmd::ReportFrameMdocOpen(void)
{
  int index;

  index = mWinApp->mDocWnd->GetFrameAdocIndex() >= 0 ? 1 : 0;
  SetRepValsAndVars(1, index);
  mLogRpt.Format("Autodoc for frames %s open", index ? "IS" : "is NOT");
  return 0;
}

// DeferWritingFrameMdoc
int CMacCmd::DeferWritingFrameMdoc(void)
{
  mWinApp->mDocWnd->SetDeferWritingFrameMdoc(true);
  return 0;
}

// OpenFrameMdoc
int CMacCmd::OpenFrameMdoc(void)
{
  CString report;

  SubstituteVariables(&mStrLine, 1, mStrLine);
  if (mWinApp->mDocWnd->GetFrameAdocIndex() >= 0)
    ABORT_LINE("The frame mdoc file is already open for line:\n\n");
  if (CheckConvertFilename(mStrItems, mStrLine, 1, report))
    return 1;
  if (mWinApp->mDocWnd->DoOpenFrameMdoc(report))
    SUSPEND_LINE("because of error opening frame mdoc file in statement:\n\n")
  return 0;
}

// CloseFrameMdoc
int CMacCmd::CloseFrameMdoc(void)
{
  if (mItemInt[1] && mWinApp->mDocWnd->GetFrameAdocIndex() < 0)
    ABORT_LINE("There is no frame mdoc file open for line:\n\n");
  mWinApp->mDocWnd->DoCloseFrameMdoc();
  return 0;
}

// AddToNextFrameStackMdoc, StartNextFrameStackMdoc
int CMacCmd::AddToNextFrameStackMdoc(void)
{
  CString report;
  bool doBack;

  doBack = CMD_IS(STARTNEXTFRAMESTACKMDOC);
  SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  if (!mRunningScrpLang)
    mParamIO->ParseString(mStrLine, mStrItems, MAX_MACRO_TOKENS, mParseQuotes);
  if (mCamera->AddToNextFrameStackMdoc(mStrItems[1], mStrCopy, doBack, &report))
    ABORT_LINE(report + " in:\n\n");
  return 0;
}

// AlignWholeTSOnly
int CMacCmd::AlignWholeTSOnly(void)
{
  int index;

  index  = (mItemEmpty[1] || mItemInt[1] != 0) ? 1 : 0;
  if (mCamera->IsConSetSaving(&mConSets[RECORD_CONSET], RECORD_CONSET, mCamParams, false)
    && (mConSets[RECORD_CONSET].alignFrames || !index) &&
    mConSets[RECORD_CONSET].useFrameAlign > 1 && mCamera->GetAlignWholeSeriesInIMOD()) {
      if (index && !mWinApp->mStoreMRC)
        ABORT_LINE("There must be an output file before this command can be used:\n\n");
      if (index && mWinApp->mStoreMRC->GetAdocIndex() < 0)
        ABORT_NOLINE("The output file was not opened with an associated .mdoc\r\n"
          "file, which is required to align whole tilt series in IMOD");
      SaveControlSet(RECORD_CONSET);
      mConSets[RECORD_CONSET].alignFrames = 1 - index;
      mAlignWholeTSOnly = index > 0;
  } else
    index = 0;
  SetReportedValues(index);
  return 0;
}

// WriteComForTSAlign
int CMacCmd::WriteComForTSAlign(void)
{
  if (mAlignWholeTSOnly) {
    mConSets[RECORD_CONSET].alignFrames = 1;
    if (mCamera->MakeMdocFrameAlignCom(""))
      ABORT_NOLINE("Problem writing com file for aligning whole tilt series");
    mConSets[RECORD_CONSET].alignFrames = 0;
  }
  return 0;
}

// SaveLogOpenNew, CloseLogOpenNew
int CMacCmd::SaveLogOpenNew(void)
{
  CString report;
  BOOL truth;

  truth = CMD_IS(CLOSELOGOPENNEW);
  if (mWinApp->mLogWindow) {
    report = mWinApp->mLogWindow->GetSaveFile();
    if (!report.IsEmpty())
      mWinApp->mLogWindow->DoSave();
    else if ((!truth || mItemEmpty[1] || !mItemInt[1]) &&
      mWinApp->mLogWindow->AskIfSave("closing and opening it again?")) {
      AbortMacro();
      return 1;
    }
    mWinApp->mLogWindow->SetUnsaved(false);
      mWinApp->mLogWindow->CloseLog();
  }
  mWinApp->AppendToLog(mWinApp->mDocWnd->DateTimeForTitle());
  if (!mItemEmpty[1] && !truth) {
    SubstituteLineStripItems(mStrLine, 1, report);
    mWinApp->mLogWindow->UpdateSaveFile(true, report);
  }
  return 0;
}

// SaveLog
int CMacCmd::SaveLog(void)
{
  CString report;

  if (!mWinApp->mLogWindow)
    ABORT_LINE("The log window must already be open for line:\n\n");
  if (mItemEmpty[2]) {
    report = mWinApp->mLogWindow->GetSaveFile();
    if (!report.IsEmpty())
      mWinApp->mLogWindow->DoSave();
    else
      mWinApp->OnFileSavelog();
  } else {
    SubstituteLineStripItems(mStrLine, 2, report);
    mWinApp->mLogWindow->UpdateSaveFile(true, report, true, mItemInt[1] != 0);
    mWinApp->mLogWindow->DoSave();
  }
  return 0;
}

// DeferLogUpdates
int CMacCmd::DeferLogUpdates(void)
{
  mDeferLogUpdates = mItemEmpty[1] || mItemInt[1] != 0;;
  if (!mDeferLogUpdates && mWinApp->mLogWindow)
    mWinApp->mLogWindow->FlushDeferredLines();
  return 0;
}

// SaveCalibrations
int CMacCmd::SaveCalibrations(void)
{
  if (mWinApp->GetAdministrator())
    mWinApp->mDocWnd->SaveCalibrations();
  else
    mWinApp->AppendToLog("Calibrations NOT saved from script, administrator mode "
     "not enabled");
  return 0;
}

// SaveSettings
int CMacCmd::SaveSettings(void)
{
  if (mWinApp->mDocWnd->ExtSaveSettings())
    ABORT_LINE("Failed to save settings for line:\n\n");
  return 0;
}

// LoadSettingsFile
int CMacCmd::LoadSettingsFile(void)
{
  CString curFile = mWinApp->mDocWnd->GetCurrentSettingsPath();
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  
  if (mWinApp->mDocWnd->GetSettingsOpen() && !curFile.IsEmpty())
    mWinApp->mParamIO->WriteSettings(curFile);
  mWinApp->mDocWnd->ReadNewSettingsFile(mStrCopy);
  return 0;
}

// SetProperty
int CMacCmd::SetProperty(void)
{
  if (mParamIO->MacroSetProperty(mStrItems[1], mItemDbl[2]))
    AbortMacro();
  return 0;
}

// ReportUserSetting, ReportProperty
int CMacCmd::ReportUserSetting(void)
{
  BOOL truth;
  double delX;

  truth = CMD_IS(REPORTPROPERTY);
  mStrCopy = truth ? "property" : "user setting";
  if ((!truth && mParamIO->MacroGetSetting(mStrItems[1], delX)) ||
    (truth && mParamIO->MacroGetProperty(mStrItems[1], delX)))
    ABORT_LINE(mStrItems[1] + " is not a recognized " + mStrCopy + " or cannot be "
    "accessed by script command in:\n\n");
  SetRepValsAndVars(2, delX);
  mLogRpt.Format("Value of %s %s is %g", (LPCTSTR)mStrCopy, (LPCTSTR)mStrItems[1], delX);
  return 0;
}

// ReportCameraProperty, SetCameraProperty
int CMacCmd::ReportCameraProperty(void)
{
  double value;
  bool setProp = CMD_IS(SETCAMERAPROPERTY);
  if (mItemInt[1]) {
    if (mItemInt[1] < 0 || mItemInt[1] > mWinApp->GetActiveCamListSize())
      ABORT_LINE("Active camera number is out of range in:\n\n")
      mCamParams = mWinApp->GetCamParams() + mActiveList[mItemInt[1] - 1];
  }
  if ((setProp && mParamIO->MacroSetCamProperty(mCamParams, mStrItems[2], mItemDbl[3])) ||
    (!setProp && mParamIO->MacroGetCamProperty(mCamParams, mStrItems[2], value)))
      ABORT_LINE(mStrItems[2] + " is not a recognized camera property or cannot be "
        "accessed by script command in:\n\n");
  if (!setProp)
    SetRepValsAndVars(3, value);
  return 0;
}

// SetUserSetting
int CMacCmd::SetUserSetting(void)
{
  BOOL truth;
  double delX;
  int index;

  if (mParamIO->MacroGetSetting(mStrItems[1], delX) ||
    mParamIO->MacroSetSetting(mStrItems[1], mItemDbl[2]))
      ABORT_LINE(mStrItems[1] + " is not a recognized setting or cannot be set by "
      "script command in:\n\n");
  mWinApp->UpdateWindowSettings();

  // See if property already saved
  truth = false;
  for (index = 0; index < (int)mSavedSettingNames.size(); index++) {
    if (mStrItems[1].CompareNoCase(mSavedSettingNames[index].c_str()) == 0) {
      truth = true;
      break;
    }
  }

  // If old value should be saved, either replace existing new value or push old and new
  if (mItemEmpty[3] || !mItemInt[3]) {
    if (truth) {
      mNewSettingValues[index] = mItemDbl[2];
    } else {
      mSavedSettingNames.push_back(std::string((LPCTSTR)mStrItems[1]));
      mSavedSettingValues.push_back(delX);
      mNewSettingValues.push_back(mItemDbl[2]);
    }
  } else if (truth) {

    // Otherwise get rid of a saved value if new value being kept
    mSavedSettingNames.erase(mSavedSettingNames.begin() + index);
    mSavedSettingValues.erase(mSavedSettingValues.begin() + index);
    mNewSettingValues.erase(mNewSettingValues.begin() + index);
  }
  return 0;
}

// Copy
int CMacCmd::Copy(void)
{
  CString report;
  EMimageBuffer *imBuf;
  int index, index2;

  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report, true))
    ABORT_LINE(report);
  if (ConvertBufferLetter(mStrItems[2], -1, false, index2, report))
    ABORT_LINE(report);
  imBuf = ImBufForIndex(index);
  if (mBufferManager->CopyImBuf(imBuf, &mImBufs[index2]))
    SUSPEND_LINE("because of buffer copy failure in statement: \n\n");
  return 0;
}

// Show
int CMacCmd::Show(void)
{
  CString report;
  int index;

  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report))
    ABORT_LINE(report);
  mWinApp->SetCurrentBuffer(index);
  return 0;
}

// ReportCurrentBuffer
int CMacCmd::ReportCurrentBuffer(void)
{
  int index, isImage;
  CString letString;
  EMimageBuffer *imBuf;

  index = mWinApp->mMainView->GetImBufIndex();
  imBuf = ImBufForIndex(index);
  isImage = (imBuf->mImage != NULL) ? 1 : 0 ;
  char letter = 'A' + index;
  letString = letter;
  SetOneReportedValue(letString, 1);
  SetOneReportedValue(isImage, 2);
  mLogRpt.Format("Current buffer is %s and%s empty", letString, isImage ? " not" : "");
  return 0;
}

// ReportActiveViewTitle
int CMacCmd::ReportActiveViewTitle(void)
{
  CChildFrame *parent = (CChildFrame *)(mWinApp->mActiveView->GetParent());
  parent->GetWindowTextA(mStrCopy);
  mLogRpt = "The active view is:" + mStrCopy;
  SetOneReportedValue(&mStrItems[1], mStrCopy, 0);
  return 0;
}

// SetActiveViewByTitle
int CMacCmd::SetActiveViewByTitle(void)
{
  CString title;
  int repval = 1;
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);

  // Here is how to get the MDI client of mainFrame and loop on its child frames
  HWND hWnd = mWinApp->mMainFrame->m_hWndMDIClient;
  CWnd* clientWnd = CWnd::FromHandle(hWnd);
  for (CWnd *pWnd = clientWnd->GetWindow(GW_CHILD); pWnd != NULL;
    pWnd = pWnd->GetNextWindow(GW_HWNDNEXT)) {
    CChildFrame *child = (CChildFrame *)pWnd;
    child->GetWindowTextA(title);
    if (title == mStrCopy) {
      repval = 0;
      child->MDIActivate();
      break;
    }
  }
  SetReportedValues(repval);
  return 0;
}

// ChangeFocus
int CMacCmd::ChangeFocus(void)
{
  if (mItemDbl[1] == 0.0 || mItemEmpty[1] || fabs(mItemDbl[1]) > 1000.)
    ABORT_LINE("Improper focus change in statement: \n\n");
  mScope->IncDefocus(mItemDbl[1]);
  return 0;
}

// SetDefocus
int CMacCmd::SetDefocus(void)
{
  if (mItemEmpty[1] || fabs(mItemDbl[1]) > 5000.)
    ABORT_LINE("Improper focus setting in statement: \n\n");
  mScope->SetDefocus(mItemDbl[1]);
  return 0;
}

// SetStandardFocus, SetAbsoluteFocus
int CMacCmd::SetStandardFocus(void)
{
  mScope->SetFocus(mItemDbl[1]);
  return 0;
}

// SetEucentricFocus
int CMacCmd::SetEucentricFocus(void)
{
  double delX;
  int index;
  double *focTab = mScope->GetLMFocusTable();
  index = mScope->GetMagIndex();
  if (index <= 0 || mWinApp->GetSTEMMode())
    ABORT_NOLINE("You cannot set eucentric focus in diffraction or STEM mode");
  delX = focTab[index * 2 + mScope->GetProbeMode()];
  if (delX < -900.)
    delX = mScope->GetStandardLMFocus(index);
  if (delX < -900.) {
    mLogRpt = "There is no standard/eucentric focus defined for the current mag range";
    if (mItemEmpty[1] || !mItemInt[1])
      ABORT_NOLINE(mLogRpt);
    index = -1;
  } else {
    mScope->SetFocus(delX);
    if (focTab[index * 2 + mScope->GetProbeMode()] < -900. && !JEOLscope &&
      index >= mScope->GetLowestMModeMagInd())
      mWinApp->AppendToLog("WARNING: Setting eucentric focus using a calibrated "
        "standard focus\r\n   from the nearest mag, not the current mag");
    index = 0;
  }
  SetReportedValues(index);
  return 0;
}

// CalEucentricFocus
int CMacCmd::CalEucentricFocus(void)
{
  if (mWinApp->mMenuTargets.DoCalibrateStandardLMfocus(true))
    ABORT_NOLINE("You cannot calibrate eucentric focus in diffraction or STEM mode");
  return 0;
}

// IncTargetDefocus
int CMacCmd::IncTargetDefocus(void)
{
  double delX;

  if (fabs(mItemDbl[1]) > 100.)
    ABORT_LINE("Change in target defocus too large in statement: \n\n");
  delX = mFocusManager->GetTargetDefocus();
  mFocusManager->SetTargetDefocus((float)(delX + mItemDbl[1]));
  mWinApp->mAlignFocusWindow.UpdateSettings();
  return 0;
}

// SetTargetDefocus
int CMacCmd::SetTargetDefocus(void)
{
  if (mItemDbl[1] < -200. || mItemDbl[1] > 50.)
    ABORT_LINE("Target defocus too large in statement: \n\n");
  mFocusManager->SetTargetDefocus(mItemFlt[1]);
  mWinApp->mAlignFocusWindow.UpdateSettings();
  return 0;
}

// CycleTargetDefocus
int CMacCmd::CycleTargetDefocus(void)
{
  float target = mFocusManager->GetTargetDefocus();
  float step, origTarget = target, startDef = mItemFlt[1], endDef = mItemFlt[2];
  float minDef = B3DMIN(startDef, endDef);
  float maxDef = B3DMAX(startDef, endDef);

  if (mItemInt[3] <= 0)
    ABORT_LINE("Number of steps must be positive for line:\n\n");
  step = (endDef - startDef) / mItemInt[3];
  if (target < minDef - 0.2 * fabs(step)) {
    target = minDef;
  } else if (target > maxDef + 0.2 * fabs(step)) {
    target = maxDef;
  } else {
    B3DCLAMP(target, minDef, maxDef);
    target += step;
    if (target < minDef - 0.2 * fabs(step))
      target = maxDef;
    else if (target > maxDef + 0.2 * fabs(step))
      target = minDef;
  }
  mFocusManager->SetTargetDefocus(target);
  if (mItemInt[4])
    mScope->IncDefocus(target - origTarget);
  mLogRpt.Format("Target defocus set to %.2f microns", target);
  return 0;
}
// ReportAutofocusOffset
int CMacCmd::ReportAutofocusOffset(void)
{
  double delX;

  delX = mFocusManager->GetDefocusOffset();
  mStrCopy.Format("Autofocus offset is: %.2f um", delX);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetRepValsAndVars(1, delX);
  return 0;
}

// SetAutofocusOffset
int CMacCmd::SetAutofocusOffset(void)
{
  if (mItemDbl[1] < -200. || mItemDbl[1] > 200.)
    ABORT_LINE("Autofocus offset too large in statement: \n\n");
  if (mFocusOffsetToRestore < -9000.) {
    mFocusOffsetToRestore = mFocusManager->GetDefocusOffset();
    mNumStatesToRestore++;
  }
  mFocusManager->SetDefocusOffset(mItemFlt[1]);
  return 0;
}

// SetObjFocus
int CMacCmd::SetObjFocus(void)
{
  CString report;
  double delX, delY;
  int index;

  index = mItemInt[1];
  delX = mScope->GetDefocus();
  mScope->SetObjFocus(index);
  delY = mScope->GetDefocus();
  report.Format("Defocus before = %.4f   after = %.4f", delX, delY);
  mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
  return 0;
}

// SetDiffractionFocus
int CMacCmd::SetDiffractionFocus(void)
{
  mScope->SetDiffractionFocus(mItemDbl[1]);
  UpdateLDAreaIfSaved();
  return 0;
}

// ResetDefocus
int CMacCmd::ResetDefocus(void)
{
  if (!mScope->ResetDefocus())
    AbortMacro();
  return 0;
}

// SetMag
int CMacCmd::SetMag(void)
{
  int index, i;

  index = B3DNINT(mItemDbl[1]);
  if (!mItemEmpty[2] && mWinApp->GetSTEMMode()) {
    mScope->SetSTEMMagnification(mItemDbl[1]);
  } else {
    i = FindIndexForMagValue(index, -1, -2);
    if (!i)
      ABORT_LINE("The value is not near enough to an existing mag in:\n\n");
    mScope->SetMagOrAdjustLDArea(i);
    UpdateLDAreaIfSaved();
  }
  return 0;
}

// SetMagIndex
int CMacCmd::SetMagIndex(void)
{
  double delX;
  int index;

  index = mItemInt[1];
  if (index <= 0 || index >= MAX_MAGS)
    ABORT_LINE("Invalid magnification index in:\n\n");
  delX = mCamParams->GIF ? mMagTab[index].EFTEMmag : mMagTab[index].mag;
  if (mCamParams->STEMcamera)
    delX =  mMagTab[index].STEMmag;
  if (!delX)
    ABORT_LINE("There is a zero in the magnification table at the index given in:\n\n");
  mScope->SetMagOrAdjustLDArea(index);
  UpdateLDAreaIfSaved();
  return 0;
}

// ChangeMag, IncMagIfFoundPixel
int CMacCmd::ChangeMag(void)
{
  BOOL truth;
  int index, index2, ix0, iy0;

  index = mScope->GetMagIndex();
  if (!index)
    SUSPEND_NOLINE("because you cannot use ChangeMag in diffraction mode");
  iy0 = mCurrentCam;
  ix0 = B3DNINT(mItemDbl[1]);
  if (!CMD_IS(CHANGEMAG)) {
    ix0 = mItemDbl[1] < 0 ? - 1 : 1;
    truth = mProcessImage->GetFoundPixelSize(iy0, index) > 0.;
    SetReportedValues(truth ? 1. : 0.);
  }
  if (CMD_IS(CHANGEMAG) || truth) {
    index2 = mWinApp->FindNextMagForCamera(iy0, index, ix0);
    if (index2 < 0)
      ABORT_LINE("Improper mag change in statement: \n\n");
    mScope->SetMagOrAdjustLDArea(index2);
  }
  UpdateLDAreaIfSaved();
  return 0;
}

// ChangeMagAndIntensity, SetMagAndIntensity
int CMacCmd::ChangeMagAndIntensity(void)
{
  double delISX, delISY, delX, delY;
  int index, index2, i;

  // Get starting and ending mag
  index = mScope->GetMagIndex();
  if (!index || mScope->GetSTEMmode())
    SUSPEND_NOLINE("because you cannot use Change/SetMagAndIntensity in diffraction"
    " or STEM mode");
  if (CMD_IS(CHANGEMAGANDINTENSITY)) {
    index2 = mWinApp->FindNextMagForCamera(mCurrentCam, index,
      B3DNINT(mItemDbl[1]));
    if (mItemEmpty[1] || index2 < 1)
      ABORT_LINE("Improper mag change in statement: \n\n");
  } else {
    i = B3DNINT(mItemDbl[1]);
    index2 = FindIndexForMagValue(i, -1, -2);
    if (!index2)
      ABORT_LINE("The value is not near enough to an existing mag in:\n\n");
  }

  // Get the intensity and change in intensity
  delISX = mScope->GetIntensity();
  delISX = mScope->GetC2Percent(mScope->FastSpotSize(), delISX);
  i = mCurrentCam;
  delISY = pow((double)mShiftManager->GetPixelSize(i, index) /
    mShiftManager->GetPixelSize(i, index2), 2.);
  i = mWinApp->mBeamAssessor->AssessBeamChange(delISY, delX, delY, -1);
  if (CheckIntensityChangeReturn(i))
    return 1;

  // Change the mag then the intensity
  mScope->SetMagOrAdjustLDArea(index2);
  if (!i)
    mScope->DelayedSetIntensity(delX, GetTickCount());
  mStrCopy.Format("%s before mag change %.3f%s, remaining factor of change "
    "needed %.3f", mScope->GetC2Name(), delISX, mScope->GetC2Units(), delY);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetReportedValues(delISX, delY);
  UpdateLDAreaIfSaved();
  return 0;
}

// SetCamLenIndex
int CMacCmd::SetCamLenIndex(void)
{
  int index;

  index = B3DNINT(mItemDbl[1]);
  if (mItemEmpty[1] || index < 1)
    ABORT_LINE("Improper camera length index in statement: \n\n");
  if (!mScope->SetCamLenIndex(index))
    ABORT_LINE("Error setting camera length index in statement: \n\n");
  UpdateLDAreaIfSaved();
  return 0;
}

// SetSpotSize
int CMacCmd::SetSpotSize(void)
{
  int index;

  index = B3DNINT(mItemDbl[1]);
  if (mItemEmpty[1] || index < mScope->GetMinSpotSize() ||
    index > mScope->GetNumSpotSizes())
    ABORT_LINE("Improper spot size in statement: \n\n");
  if (!mScope->SetSpotSize(index)) {
    AbortMacro();
    return 1;
  }
  UpdateLDAreaIfSaved();
  return 0;
}

// SetProbeMode
int CMacCmd::SetProbeMode(void)
{
  int index;

  if (mItem1upper.Find("MICRO") == 0 || mItem1upper == "1")
    index = 1;
  else if (mItem1upper.Find("NANO") == 0 || mItem1upper == "0")
    index = 0;
  else
    ABORT_LINE("Probe mode must be 0, 1, nano, or micro in statement: \n\n");
  if (!mScope->SetProbeOrAdjustLDArea(index)) {
    AbortMacro();
    return 1;
  }
  UpdateLDAreaIfSaved();
  return 0;
}

// Delay
int CMacCmd::Delay(void)
{
  double delISX;

  delISX = mItemDbl[1];
  if (!mStrItems[2].CompareNoCase("MSEC"))
    mSleepTime = delISX;
  else if (!mStrItems[2].CompareNoCase("SEC"))
    mSleepTime = 1000. * delISX;
  else if (!mStrItems[2].CompareNoCase("MIN"))
    mSleepTime = 60000. * delISX;
  else if (delISX > 60)
    mSleepTime = delISX;
  else
    mSleepTime = 1000. * delISX;
  mSleepStart = GetTickCount();
  if (mSleepTime > 3000)
    mWinApp->SetStatusText(SIMPLE_PANE, "WAITING FOR DELAY");
  return 0;
}

// WaitForMidnight
int CMacCmd::WaitForMidnight(void)
{
  CString report;
  double delISX, delISY, delX, delY;
  int ix0, ix1;
  // Get the times before and after the target time and the alternative target
  delX = delY = 5.;
  if (!mItemEmpty[1])
    delX = mItemDbl[1];
  if (!mItemEmpty[2])
    delY = mItemDbl[2];
  ix0 = 24;
  ix1 = 0;
  if (!mItemEmpty[3] && !mItemEmpty[4]) {
    ix0 = mItemInt[3];
    ix1 = mItemInt[4];
  }

  // Compute minutes into the day and the interval from now to the target
  CTime time = CTime::GetCurrentTime();
  delISX = time.GetHour() * 60 + time.GetMinute() + time.GetSecond() / 60.;
  delISY = ix0 * 60 + ix1 - delISX;

  // If wthin the window at all, set up the sleep
  if (delISY + delY > 0 && delISY < delX) {
    mSleepTime = 60000. * (delISY + delY);
    mSleepStart = GetTickCount();
    report.Format("Sleeping until %.1f minutes after ", delY);
    mStrCopy = "midnight";
    if (!mItemEmpty[4])
      mStrCopy.Format("%02d:%02d", ix0, ix1);
    mWinApp->AppendToLog(report + mStrCopy, mLogAction);
    mStrCopy.MakeUpper();
    report.Format(" + %.0f", delY);
    mWinApp->SetStatusText(SIMPLE_PANE, "WAIT TO " + mStrCopy);
  }
  return 0;
}

// WaitForDose
int CMacCmd::WaitForDose(void)
{
  mDoseTarget = mItemDbl[1];
  mNumDoseReports = 10;
  if (!mItemEmpty[2])
    mNumDoseReports = B3DMAX(1, mItemInt[2]);
  mWinApp->mScopeStatus.SetWatchDose(true);
  mDoseStart = mWinApp->mScopeStatus.GetFullCumDose();
  mDoseNextReport = mDoseTarget / (mNumDoseReports * 10);
  mDoseTime = GetTickCount();
  mWinApp->SetStatusText(SIMPLE_PANE, "WAITING FOR DOSE");
  return 0;
}

// ScreenUp
int CMacCmd::ScreenUp(void)
{
  mScope->SetScreenPos(spUp);
  mMovedScreen = true;
  return 0;
}

// ScreenDown
int CMacCmd::ScreenDown(void)
{
  mScope->SetScreenPos(spDown);
  mMovedScreen = true;
  return 0;
}

// ReportScreen
int CMacCmd::ReportScreen(void)
{
  int index;

  index = mScope->GetScreenPos() == spDown ? 1 : 0;
  mLogRpt.Format("Screen is %s", index ? "DOWN" : "UP");
  SetRepValsAndVars(1, index);
  return 0;
}

// ReportScreenCurrent
int CMacCmd::ReportScreenCurrent(void)
{
  double delX;

  delX = mScope->GetScreenCurrent();
  mLogRpt.Format("Screen current is %.3f nA", delX);
  SetRepValsAndVars(1, delX);
  return 0;
}

// ImageShiftByPixels
int CMacCmd::ImageShiftByPixels(void)
{
  CString report;
  ScaleMat bInv;
  double delISX, delISY;
  int index, index2, ix1;

  bInv = mShiftManager->CameraToIS(mScope->GetMagIndex());
  ix1 = BinDivisorI(mCamParams);
  index = B3DNINT(mItemDbl[1] * ix1);
  index2 = B3DNINT(mItemDbl[2] * ix1);

  delISX = bInv.xpx * index + bInv.xpy * index2;
  delISY = bInv.ypx * index + bInv.ypy * index2;
  if (TestIncrementalImageShift(delISX, delISY))
    return 0;
  if (AdjustBTApplyISSetDelay(delISX, delISY, mItemInt[4],
    !mItemEmpty[3], mItemDbl[3], report))
    ABORT_LINE(report);
  return 0;
}

// ImageShiftByUnits
int CMacCmd::ImageShiftByUnits(void)
{
  CString report;
  ScaleMat aMat;
  double delISX, delISY, delX, delY, specDist;

  delISX = mItemDbl[1];
  delISY = mItemDbl[2];
  if (TestIncrementalImageShift(delISX, delISY))
    return 0;
  if (AdjustBTApplyISSetDelay(delISX, delISY, mItemInt[4],
    !mItemEmpty[3], mItemDbl[3], report))
    ABORT_LINE(report);

  // Report distance on specimen
  aMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
  delX = aMat.xpx * delISX + aMat.xpy * delISY;
  delY = aMat.ypx * delISX + aMat.ypy * delISY;
  specDist = 1000. * sqrt(delX * delX + delY * delY);
  mStrCopy.Format("%.1f nm shifted on specimen", specDist);
  mWinApp->AppendToLog(mStrCopy, LOG_OPEN_IF_CLOSED);
  return 0;
}

// ImageShiftByMicrons
int CMacCmd::ImageShiftByMicrons(void)
{
  CString report;
  ScaleMat aMat, bInv;
  double delISX, delISY, delX, delY;

  delX = mItemDbl[1];
  delY = mItemDbl[2];
  aMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
  bInv = mShiftManager->MatInv(aMat);
  delISX = bInv.xpx * delX + bInv.xpy * delY;
  delISY = bInv.ypx * delX + bInv.ypy * delY;
  if (TestIncrementalImageShift(delISX, delISY))
    return 0;
  if (AdjustBTApplyISSetDelay(delISX, delISY, mItemInt[4],
    !mItemEmpty[3], mItemDbl[3], report))
    ABORT_LINE(report);
  return 0;
}

// ImageShiftByStageDiff
int CMacCmd::ImageShiftByStageDiff(void)
{
  CString report;
  ScaleMat aMat, bInv;
  double delISX, delISY;
  int index;
  float backlashX, backlashY;

  mShiftManager->GetStageTiltFactors(backlashX, backlashY);
  index = mScope->GetMagIndex();
  aMat = mShiftManager->StageToCamera(mCurrentCam, index);
  bInv = MatMul(aMat, mShiftManager->CameraToIS(index));
  ApplyScaleMatrix(bInv, mItemFlt[1] * backlashX,
    mItemFlt[2] *backlashY, delISX, delISY);
  if (TestIncrementalImageShift(delISX, delISY))
    return 0;
  if (AdjustBTApplyISSetDelay(delISX, delISY, mItemInt[4],
    !mItemEmpty[3], mItemDbl[3], report))
    ABORT_LINE(report);
  return 0;
}

// ImageShiftToLastMultiHole
int CMacCmd::ImageShiftToLastMultiHole(void)
{
  float backlashX, backlashY;

  mWinApp->mParticleTasks->GetLastHoleImageShift(backlashX, backlashY);
  mScope->IncImageShift(backlashX, backlashY);
  return 0;
}

// ShiftImageForDrift
int CMacCmd::ShiftImageForDrift(void)
{
  mCamera->QueueDriftRate(mItemDbl[1], mItemDbl[2], mItemInt[3] != 0);
  return 0;
}

// ShiftCalSkipLensNorm
int CMacCmd::ShiftCalSkipLensNorm(void)
{
  mWinApp->mShiftCalibrator->SetSkipLensNormNextIScal(mItemEmpty[1] || mItemInt[1] != 0);
  return 0;
}

// CalibrateImageShift
int CMacCmd::CalibrateImageShift(void)
{
  int index;

  index = 0;
  if (mItemInt[1])
    index = -1;
  mWinApp->mShiftCalibrator->CalibrateIS(index, false, true);
  return 0;
}

// CalibrateHighFocusIS
int CMacCmd::CalibrateHighFocusIS(void)
{
  if (mItemFlt[1] > 0)
    ABORT_LINE("The current defocus should be negative for line:\n\n");
  if (!mItemEmpty[2] && mItemDbl[2])
    mScope->IncDefocus(mItemDbl[2]);
  if (mWinApp->mShiftCalibrator->CalibrateISatHighDefocus(false, mItemFlt[1])) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// ReportFocusDrift
int CMacCmd::ReportFocusDrift(void)
{
  double delX, delY;

  if (mFocusManager->GetLastDrift(delX, delY))
    ABORT_LINE("No drift available from last autofocus for statement: \n\n");
  mStrCopy.Format("Last drift in autofocus: %.3f %.3f nm/sec", delX, delY);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetRepValsAndVars(1, delX, delY);
  return 0;
}

// TestSTEMshift
int CMacCmd::TestSTEMshift(void)
{
  mScope->TestSTEMshift(mItemInt[1], mItemInt[2], mItemInt[3]);
  return 0;
}

// QuickFlyback
int CMacCmd::QuickFlyback(void)
{
  int index;

  index = CString("VFTRP").Find(mStrItems[1].Left(1));
  if (index < 0)
    ABORT_NOLINE("QuickFlyback must be followed by one of V, F, T, R, or P");
  if (!(mCamParams->STEMcamera && mCamParams->FEItype))
    ABORT_NOLINE("QuickFlyback can be run only if the current camera is an FEI STEM"
    " camera")
  mWinApp->mCalibTiming->CalibrateTiming(index, mItemFlt[2], false);
  return 0;
}

// ReportAutoFocus
int CMacCmd::ReportAutoFocus(void)
{
  double delX;
  int index, index2;

  delX = mFocusManager->GetCurrentDefocus();
  index = mFocusManager->GetLastFailed() ? - 1 : 0;
  index2 = mFocusManager->GetLastAborted();
  if (index2)
    index = index2;
  if (index)
    mStrCopy.Format("Last autofocus FAILED with error type %d", index);
  else
    mStrCopy.Format("Last defocus in autofocus: %.2f um", delX);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetRepValsAndVars(1, delX, (double)index);
  return 0;
}

// ReportTargetDefocus
int CMacCmd::ReportTargetDefocus(void)
{
  double delX;

  delX = mFocusManager->GetTargetDefocus();
  mStrCopy.Format("Target defocus is: %.2f um", delX);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetRepValsAndVars(1, delX);
  return 0;
}

// SetBeamShift
int CMacCmd::SetBeamShift(void)
{
  double delX, delY;

  delX = mItemDbl[1];
  delY = mItemDbl[2];
  if (!mScope->SetBeamShift(delX, delY)) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// MoveBeamByMicrons
int CMacCmd::MoveBeamByMicrons(void)
{
  if (mProcessImage->MoveBeam(NULL, mItemFlt[1], mItemFlt[2]))
    ABORT_LINE("Either an image shift or a beam shift calibration is not available for"
    " line:\n\n");
  return 0;
}

// MoveBeamByFieldFraction
int CMacCmd::MoveBeamByFieldFraction(void)
{
  if (mProcessImage->MoveBeamByCameraFraction(mItemFlt[1],
    mItemFlt[2]))
    ABORT_LINE("Either an image shift or a beam shift calibration is not available for"
    " line:\n\n");
  return 0;
}

// SetBeamTilt
int CMacCmd::SetBeamTilt(void)
{
  if (!mScope->SetBeamTilt(mItemDbl[1], mItemDbl[2])) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// ReportBeamShift
int CMacCmd::ReportBeamShift(void)
{
  double delX, delY;

  if (!mScope->GetBeamShift(delX, delY)) {
    AbortMacro();
    return 1;
  }
  mStrCopy.Format("Beam shift %.3f %.3f (putative microns)", delX, delY);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetRepValsAndVars(1, delX, delY);
  return 0;
}

// ReportBeamTilt
int CMacCmd::ReportBeamTilt(void)
{
  double delX, delY;

  if (!mScope->GetBeamTilt(delX, delY)) {
    AbortMacro();
    return 1;
  }
  mStrCopy.Format("Beam tilt %.3f %.3f", delX, delY);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetRepValsAndVars(1, delX, delY);
  return 0;
}

// SetImageBeamTilt
int CMacCmd::SetImageBeamTilt(void)
{
  if (!FEIscope)
    ABORT_LINE("Image-beam tilt is available only on FEI scopes for line:\n\n");
  if (!mScope->SetImageBeamTilt(mItemDbl[1], mItemDbl[2])) {
    AbortMacro();
    return 1;
  }
  return 0;

}

// ReportImageBeamTilt
int CMacCmd::ReportImageBeamTilt(void)
{
  double delX, delY;
  if (!FEIscope)
    ABORT_LINE("Image-beam tilt is available only on FEI scopes for line:\n\n");

  if (!mScope->GetImageBeamTilt(delX, delY)) {
    AbortMacro();
    return 1;
  }
  mStrCopy.Format("Image-beam tilt %.3f %.3f", delX, delY);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetRepValsAndVars(1, delX, delY);
  return 0;
}

// ReportDiffractionShift
int CMacCmd::ReportDiffractionShift(void)
{
  double delX, delY;
  if (!mScope->GetDiffractionShift(delX, delY)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Diffraction shift is %.3f %.3f", delX, delY);
  SetRepValsAndVars(1, delX, delY);
  return 0;
}

// SetDiffractionShift
int CMacCmd::SetDiffractionShift(void)
{
  if (!mScope->SetDiffractionShift(mItemDbl[1], mItemDbl[2])) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// SetImageShift
int CMacCmd::SetImageShift(void)
{
  CString report;
  BOOL truth;
  double delISX = 0., delISY = 0., delX, delY;

  delX = mItemDbl[1];
  delY = mItemDbl[2];
  if (!mShiftManager->ImageShiftIsOK(delX, delY, false)) {
    mLogRpt.Format("Image shift %.3f %.3f is bigger than the limit and is not being done",
      delX, delY);
    SetReportedValues(1.);
    return 0;
  }
  SetReportedValues(0.);

  truth = mItemInt[4];
  if (truth)
    mScope->GetLDCenteredShift(delISX, delISY);
  if (!mScope->SetLDCenteredShift(delX, delY)) {
    AbortMacro();
    return 1;
  }
  if (AdjustBeamTiltIfSelected(delX - delISX, delY - delISY, truth, report))
    ABORT_LINE(report);
  if (!mItemEmpty[3] && mItemDbl[3] > 0)
    mShiftManager->SetISTimeOut(mItemFlt[3] *mShiftManager->GetLastISDelay());
  return 0;
}

// AdjustBeamTiltforIS
int CMacCmd::AdjustBeamTiltforIS(void)
{
  CString report;
  double delISX, delISY;

  if (!mItemEmpty[1] && mItemEmpty[2])
    ABORT_LINE("There must be either no entries or X and Y IS entries for line:\n\n");
  if (!mItemEmpty[2]) {
    delISX = mItemDbl[1];
    delISY = mItemDbl[2];
  } else
    mScope->GetLDCenteredShift(delISX, delISY);
  if (AdjustBeamTiltIfSelected(delISX, delISY, true, report))
    ABORT_LINE(report);
  return 0;
}

// ReportImageShift
int CMacCmd::ReportImageShift(void)
{
  CString report;
  ScaleMat aMat, bInv;
  double delISX, delISY, delX, delY, h1, stageX, stageY;
  int index, mag;

  if (!mScope->GetLDCenteredShift(delX, delY)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Image shift %.3f %.3f IS units", delX, delY);
  mag = mScope->GetMagIndex();
  aMat = mShiftManager->IStoCamera(mag);
  delISX = delISY = stageX = stageY = 0.;
  if (aMat.xpx) {
    index = BinDivisorI(mCamParams);
    delISX = -(delX * aMat.xpx + delY * aMat.xpy) / index;
    delISY = -(delX * aMat.ypx + delY * aMat.ypy) / index;
    h1 = cos(DTOR * mScope->GetTiltAngle());
    bInv = MatMul(aMat,
      MatInv(mShiftManager->StageToCamera(mCurrentCam, mag)));
    stageX = (bInv.xpx * delX + bInv.xpy * delY) / (HitachiScope ? h1 : 1.);
    stageY = (bInv.ypx * delX + bInv.ypy * delY) / (HitachiScope ? 1. : h1);
    report.Format("   %.1f %.1f unbinned pixels; need stage %.3f %.3f if reset", delISX,
                  delISY, stageX, stageY);
    mLogRpt += report;
  }
  SetRepValsAndVars(1, delX, delY, delISX, delISY, stageX, stageY);
  return 0;
}

// ReportTiltAxisOffset
int CMacCmd::ReportTiltAxisOffset(void)
{
  float ISX, ISY, stageX, stageY, offset = mScope->GetTiltAxisOffset();
  ScaleMat aMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex(), mCurrentCam);
  if (!aMat.xpx)
    ABORT_LINE("There is no image shift to specimen matrix available for line:\n\n");
  aMat = mShiftManager->MatInv(aMat);
  ISX = aMat.xpy * offset;
  ISY = aMat.ypy * offset;
  aMat = mShiftManager->SpecimenToStage(1., 1.);
  stageX = -aMat.xpy * offset;
  stageY = -aMat.ypy * offset;
  mLogRpt.Format("Tilt axis offset %.3f um, %.3f %.3f stage units, %.3f %.3f IS units, %s"
    " applied", offset, stageX, stageY, ISX, ISY, mScope->GetShiftToTiltAxis() ? 
    "IS BEING" : "is NOT being");
  SetRepValsAndVars(1, offset, stageX, stageY, ISX, ISY, mScope->GetShiftToTiltAxis() ? 
    1 : 0);
  return 0;
}

// SetObjectiveStigmator
int CMacCmd::SetObjectiveStigmator(void)
{
  double delX, delY;

  delX = mItemDbl[1];
  delY = mItemDbl[2];
  if (!mScope->SetObjectiveStigmator(delX, delY)) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// SetCondenserStigmator
int CMacCmd::SetCondenserStigmator(void)
{
  double delX, delY;

  delX = mItemDbl[1];
  delY = mItemDbl[2];
  if (!mScope->SetCondenserStigmator(delX, delY)) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// ReportXLensDeflector, SetXLensDeflector, ReportXLensFocus, SetXLensFocus
int CMacCmd::ReportXLensDeflector(void)
{
  double delX, delY;
  int index;
  const char * defNames[] = {"Shift", "Tilt", "Stigmator"};
  switch (mCmdIndex) {
  case CME_REPORTXLENSDEFLECTOR:
    index = mScope->GetXLensDeflector(mItemInt[1], delX, delY);
    if (!index) {
      mLogRpt.Format("X Lens %s is %f %f", defNames[mItemInt[1] - 1], delX, delY);
      SetRepValsAndVars(1, delX, delY);
    }
    break;
  case CME_SETXLENSDEFLECTOR:
    index = mScope->SetXLensDeflector(mItemInt[1], mItemDbl[2], mItemDbl[3]);
    break;
  case CME_REPORTXLENSFOCUS:
    index = mScope->GetXLensFocus(delX);
    if (!index) {
      mLogRpt.Format("X Lens focus is %f", delX);
      SetRepValsAndVars(1, delX);
    }
    break;
  case CME_SETXLENSFOCUS:
    index = mScope->SetXLensFocus(mItemDbl[1]);
    break;
  }
  if (index == 1) {
    ABORT_LINE("Scope is not initialized for:\n\n");
  } else if (index == 2) {
    ABORT_LINE("Plugin is missing needed function for:\n\n");
  } else if (index == 3) {
    ABORT_LINE("Deflector number must be between 1 and 3 in:\n\n");
  } else if (index == 5) {
    ABORT_LINE("There is no connection to adatl COM object for:\n\n");
  } else if (index > 5) {
    ABORT_LINE("X Mode is not available for:\n\n");
  }
  return 0;
}

// ReportSpecimenShift
int CMacCmd::ReportSpecimenShift(void)
{
  ScaleMat aMat;
  double delISX, delISY, delX, delY;

  if (!mScope->GetLDCenteredShift(delISX, delISY)) {
    AbortMacro();
    return 1;
  }
  aMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
  if (aMat.xpx) {
    delX = aMat.xpx * delISX + aMat.xpy * delISY;
    delY = aMat.ypx * delISX + aMat.ypy * delISY;
    mLogRpt.Format("Image shift is %.3f  %.3f in specimen coordinates",  delX, delY);
    SetRepValsAndVars(1, delX, delY);
  } else {
    mWinApp->AppendToLog("There is no calibration for converting image shift to "
      "specimen coordinates", mLogAction);
  }
  return 0;
}

// ReportObjectiveStigmator
int CMacCmd::ReportObjectiveStigmator(void)
{
  double delX, delY;

  if (!mScope->GetObjectiveStigmator(delX, delY)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Objective stigmator is %.5f %.5f", delX, delY);
  SetRepValsAndVars(1, delX, delY);
  return 0;
}

// ReportCondenserStigmator
int CMacCmd::ReportCondenserStigmator(void)
{
  double delX, delY;

  if (!mScope->GetCondenserStigmator(delX, delY)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Condenser stigmator is %.5f %.5f", delX, delY);
  SetRepValsAndVars(1, delX, delY);
  return 0;
}

// SuppressReports
int CMacCmd::SuppressReports(void)
{
  if (!mItemEmpty[1] && !mItemInt[1])
    mLogAction = LOG_OPEN_IF_CLOSED;
  else
    mLogAction = LOG_IGNORE;
  return 0;
}

// ErrorsToLog
int CMacCmd::ErrorsToLog(void)
{
  if (!mItemEmpty[1] && !mItemInt[1])
    mLogErrAction = LOG_IGNORE;
  else
    mLogErrAction = LOG_OPEN_IF_CLOSED;
  return 0;

}

// ReportAlignShift, ReportShiftDiffFrom, ReportISforBufferShift
int CMacCmd::ReportAlignShift(void)
{
  ScaleMat aMat, bInv;
  double delISX, delISY, delX, delY, h1, v1;
  int index, index2;
  float shiftX, shiftY;

  delISY = 0.;
  if (CMD_IS(REPORTSHIFTDIFFFROM)) {
    delISY = mItemDbl[1];
    if (mItemEmpty[1] || delISY <= 0)
      ABORT_LINE("Improper or missing comparison value in statement: \n\n");
  }
  if (mImBufs->mImage) {
    mImBufs->mImage->getShifts(shiftX, shiftY);
    shiftX *= mImBufs->mBinning;
    shiftY *= -mImBufs->mBinning;
    mAccumShiftX += shiftX;
    mAccumShiftY += shiftY;
    index = mScope->GetMagIndex();
    index2 = BinDivisorI(mCamParams);
    if (delISY) {
      delX = mShiftManager->GetPixelSize(mCurrentCam, index);
      delY = delX * sqrt(shiftX * shiftX + shiftY * shiftY);
      delISX = 100. * (delY / delISY - 1.);
      mAccumDiff += (float)(delY - delISY);
      delISY = delX * sqrt(mAccumShiftX * mAccumShiftX + mAccumShiftY * mAccumShiftY);
      mStrCopy.Format("%6.1f %% diff, cumulative diff %6.2f  um, total distance %.1f",
        delISX, mAccumDiff, delISY);
      SetRepValsAndVars(2, delISX, mAccumDiff, delISY);
    } else {
      bInv = mShiftManager->CameraToIS(index);
      aMat = mShiftManager->IStoSpecimen(index);

      // Convert to image shift units, then determine distance on specimen
      // implied by each axis of image shift separately
      delISX = bInv.xpx * shiftX + bInv.xpy * shiftY;
      delISY = bInv.ypx * shiftX + bInv.ypy * shiftY;
      if (CMD_IS(REPORTISFORBUFFERSHIFT)) {
        mStrCopy.Format("Buffer shift corresponds to %.3f %.3f IS units", -delISX,
          - delISY);
        SetRepValsAndVars(1, -delISX, -delISY);
      } else {
        h1 = 1000. * (delISX * aMat.xpx + delISY * aMat.xpy);
        v1 = 1000. * (delISX * aMat.ypx + delISY * aMat.ypy);
        delX = 1000. * sqrt(pow(delISX * aMat.xpx, 2) + pow(delISX * aMat.ypx, 2));
        delY = 1000. * sqrt(pow(delISY * aMat.xpy, 2) + pow(delISY * aMat.ypy, 2));
        mStrCopy.Format("%6.1f %6.1f unbinned pixels; %6.1f %6.1f nm along two shift "
          "axes; %6.1f %6.1f nm on specimen axes",
          shiftX / index2, shiftY / index2, delX, delY, h1, v1);
        SetRepValsAndVars(1, shiftX, shiftY, delX, delY, h1, v1);
      }
    }
    mWinApp->AppendToLog(mStrCopy, mLogAction);
  }
  return 0;
}

// ReportAccumShift
int CMacCmd::ReportAccumShift(void)
{
  int index2;

  index2 = BinDivisorI(mCamParams);
  mLogRpt.Format("%8.1f %8.1f cumulative pixels", mAccumShiftX / index2,
    mAccumShiftY / index2);
  SetRepValsAndVars(1, mAccumShiftX, mAccumShiftY);
  return 0;
}

// ResetAccumShift
int CMacCmd::ResetAccumShift(void)
{
  mAccumShiftX = 0.;
  mAccumShiftY = 0.;
  mAccumDiff = 0.;
  return 0;
}

// ReportAlignTrimming
int CMacCmd::ReportAlignTrimming(void)
{
  int ix0, ix1, iy0, iy1;

  mShiftManager->GetLastAlignTrims(ix0, ix1, iy0, iy1);
  mLogRpt.Format("Total border trimmed in last autoalign in X & Y for A: %d %d   "
    "Reference: %d %d", ix0, ix1, iy0, iy1);
  SetRepValsAndVars(1, ix0, ix1, iy0, iy1);
  return 0;
}

// CameraToISMatrix,  ISToCameraMatrix, CameraToStageMatrix, StageToCameraMatrix,
// CameraToSpecimenMatrix, SpecimenToCameraMatrix, ISToSpecimenMatrix,
// SpecimenToISMatrix, ISToStageMatrix, StageToISMatrix,
// StageToSpecimenMatrix, SpecimenToStageMatrix
int CMacCmd::CameraToISMatrix(void)
{
  CString report;
  BOOL truth;
  ScaleMat aMat, bInv;
  int index;

  index = mItemInt[1];
  if (index <= 0)
    index = mScope->GetMagIndex();
  if (!index)
    ABORT_LINE("There are no scale matrices for diffraction mode for line:\n\n");
  if (index >= MAX_MAGS)
    ABORT_LINE("The mag index is out of range in line:\n\n");
  truth = false;
  switch (mCmdIndex) {

  case CME_CAMERATOISMATRIX:
    truth = true;
  case CME_ISTOCAMERAMATRIX:
    aMat = mShiftManager->IStoGivenCamera(index, mCurrentCam);
    break;
  case CME_CAMERATOSTAGEMATRIX:
    truth = true;
  case CME_STAGETOCAMERAMATRIX:
    aMat = mShiftManager->StageToCamera(mCurrentCam, index);
    break;

  case CME_SPECIMENTOCAMERAMATRIX:
    truth = true;
  case CME_CAMERATOSPECIMENMATRIX:
    aMat = mShiftManager->CameraToSpecimen(index);
    break;

  case CME_SPECIMENTOISMATRIX:
    truth = true;
  case CME_ISTOSPECIMENMATRIX:
    aMat = aMat = mShiftManager->IStoSpecimen(index, mCurrentCam);
    break;
  case CME_STAGETOISMATRIX:
    truth = true;
  case CME_ISTOSTAGEMATRIX:
    aMat = mShiftManager->IStoGivenCamera(index, mCurrentCam);
    if (aMat.xpx) {
      bInv = mShiftManager->StageToCamera(mCurrentCam, index);
      if (bInv.xpx)
        aMat = MatMul(aMat, MatInv(bInv));
      else
        aMat.xpx = 0.;
    }
    break;
  case CME_STAGETOSPECIMENMATRIX:
    truth = true;
  case CME_SPECIMENTOSTAGEMATRIX:
    aMat = mShiftManager->SpecimenToStage(1., 1.);
    break;
  }
  if (truth && aMat.xpx)
    aMat = MatInv(aMat);
  report = cmdList[mCmdIndex].mixedCase;
  report.Replace("To", " to ");
  report.Replace("Matrix", " matrix");
  if (aMat.xpx)
    mLogRpt.Format("%s at mag index %d is %.5g  %.5g  %.5g  %.5g", (LPCTSTR)report,
      index, aMat.xpx, aMat.xpy, aMat.ypx, aMat.ypy);
  else
    mLogRpt.Format("%s is not available for mag index %d", (LPCTSTR)report, index);
  SetRepValsAndVars(2, aMat.xpx, aMat.xpy, aMat.ypx, aMat.ypy);
  return 0;
}

// ReportClock
int CMacCmd::ReportClock(void)
{
  double delX;

  delX = 0.001 * GetTickCount() - 0.001 * mStartClock;
  mLogRpt.Format("%.2f seconds elapsed time", delX);
  SetRepValsAndVars(1, delX);
  return 0;
}

// ResetClock
int CMacCmd::ResetClock(void)
{
  mStartClock = GetTickCount();
  return 0;
}

// ReportMinuteTime
int CMacCmd::ReportMinuteTime(void)
{
  CString report;
  int index;

  index = mWinApp->MinuteTimeStamp();
  if (!mItemEmpty[1] && SetVariable(mStrItems[1], (double)index,
    VARTYPE_PERSIST + VARTYPE_ADD_FOR_NUM, -1, false, &report))
      ABORT_LINE(report + "\n(This command assigns to a persistent variable):\n\n");
  mLogRpt.Format("Absolute minute time = %d", index);
  SetReportedValues(index);
  return 0;
}

// SetCustomTime, ReportCustomInterval
int CMacCmd::SetCustomTime(void)
{
  int index, index2;
  std::string sstr = (LPCTSTR)mItem1upper;
  std::map < std::string, int > ::iterator custIter;
  index = mWinApp->MinuteTimeStamp();
  if (!mItemEmpty[2])
    index = mItemInt[2];
  index2 = (int)mCustomTimeMap.count(sstr);
  if (index2)
    custIter = mCustomTimeMap.find(sstr);
  if (CMD_IS(SETCUSTOMTIME)) {

    // Insert does not replace a value!  You have to get the iterator and assign it
    if (index2)
      custIter->second = index;
    else
      mCustomTimeMap.insert(std::pair < std::string, int > (sstr, index));
    mWinApp->mDocWnd->SetShortTermNotSaved();
  } else {
    if (index2) {
      index -= custIter->second;
      mLogRpt.Format("%d minutes elapsed since custom time %s set", index,
         (LPCTSTR)mStrItems[1]);
    } else {
      index = 2 * MAX_CUSTOM_INTERVAL;
      mLogRpt = "Custom time " + mStrItems[1] + " has not been set";
    }
    SetReportedValues(index);
  }
  return 0;
}

// ReportTickTime
int CMacCmd::ReportTickTime(void)
{
  CString report;
  double delISX;

  delISX = SEMTickInterval(mWinApp->ProgramStartTime()) / 1000.;
  if (!mItemEmpty[1] && SetVariable(mStrItems[1], delISX,
    VARTYPE_PERSIST + VARTYPE_ADD_FOR_NUM, -1, false, &report))
      ABORT_LINE(report + "\n(This command assigns to a persistent variable):\n\n");
  mLogRpt.Format("Tick time from program start = %.3f", delISX);
  SetReportedValues(delISX);
  return 0;
}

// ElapsedTickTime
int CMacCmd::ElapsedTickTime(void)
{
  double delISX, delISY;

  delISY = SEMTickInterval(mWinApp->ProgramStartTime());
  delISX = SEMTickInterval(delISY, mItemDbl[1] * 1000.) / 1000.;
  mLogRpt.Format("Elapsed tick time = %.3f", delISX);
  SetRepValsAndVars(2, delISX);
  return 0;
}

// ReportDateTime
int CMacCmd::ReportDateTime(void)
{
  int index, index2;
  CTime ctDateTime = CTime::GetCurrentTime();
  index = 10000 * ctDateTime.GetYear() + 100 * ctDateTime.GetMonth() +
    ctDateTime.GetDay();
  index2 = 100 * ctDateTime.GetHour() + ctDateTime.GetMinute();
  mLogRpt.Format("%d  %04d", index, index2);
  SetRepValsAndVars(1, index, index2);
  return 0;
}

// MoveStage, MoveStageTo, MoveStageWithSpeed, TestRelaxingStage, StageShiftByPixels,
// StageToLastMultiHole
int CMacCmd::MoveStage(void)
{
  BOOL truth;
  ScaleMat aMat, bInv;
  double h1, stageX, stageY, stageZ;
  float backlashX, backlashY;
  StageMoveInfo smi;

    smi.z = 0.;
    smi.alpha = 0.;
    smi.axisBits = 0;
    smi.useSpeed = false;
    smi.backX = smi.backY = smi.relaxX = smi.relaxY = 0.;
    truth = CMD_IS(TESTRELAXINGSTAGE);

    // If stage not ready, back up and try again, otherwise do action
    if (mScope->StageBusy() > 0)
      mLastIndex = mCurrentIndex;
    else {
      if (CMD_IS(STAGESHIFTBYPIXELS)) {
        h1 = DTOR * mScope->GetTiltAngle();
        aMat = mShiftManager->StageToCamera(mCurrentCam,
          mScope->GetMagIndex());
        if (!aMat.xpx)
          ABORT_LINE("There is no stage to camera calibration available for line:\n\n");
        bInv = MatInv(aMat);
        stageX = bInv.xpx * mItemDbl[1] + bInv.xpy * mItemDbl[2];
        stageY = (bInv.ypx * mItemDbl[1] + bInv.ypy * mItemDbl[2]) / cos(h1);
        stageZ = 0.;
      } else if (CMD_IS(STAGETOLASTMULTIHOLE)) {
        mWinApp->mParticleTasks->GetLastHoleStagePos(stageX, stageY);
        if (stageX < EXTRA_VALUE_TEST)
          ABORT_LINE("The multiple Record routine has not been run for line:\n\n");
        stageZ = 0.;
      } else {

        if (mItemEmpty[2])
          ABORT_LINE("Stage movement command does not have at least 2 numbers: \n\n");
        stageX = mItemDbl[1];
        stageY = mItemDbl[2];
        if (CMD_IS(MOVESTAGEWITHSPEED)) {
          stageZ = 0.;
          smi.useSpeed = true;
          smi.speed = mItemDbl[3];
          if (smi.speed <= 0.)
            ABORT_LINE("Speed entry must be positive in line:/n/n");
        } else {
          stageZ = (mItemEmpty[3] || truth) ? 0. : mItemDbl[3];
        }
      }
      if (CMD_IS(MOVESTAGE) || CMD_IS(STAGESHIFTBYPIXELS) || CMD_IS(MOVESTAGEWITHSPEED)
        || truth) {
        if (!mScope->GetStagePosition(smi.x, smi.y, smi.z))
          SUSPEND_NOLINE("because of failure to get stage position");
          //CString report;
        //report.Format("Start at %.2f %.2f %.2f", smi.x, smi.y, smi.z);
        //mWinApp->AppendToLog(report, LOG_IF_ADMIN_OPEN_IF_CLOSED);

        // For each of X, Y, Z, set axis bit if nonzero;
        smi.x += stageX;
        if (stageX != 0.)
          smi.axisBits |= axisX;
        smi.y += stageY;
        if (stageY != 0.)
          smi.axisBits |= axisY;
        smi.z += stageZ;
        if (stageZ != 0.)
          smi.axisBits |= axisZ;
        if (truth) {
          backlashX = mItemEmpty[3] ? mWinApp->mMontageController->GetStageBacklash() :
            mItemFlt[3];
        backlashY = mItemEmpty[4] ? mScope->GetStageRelaxation() : mItemFlt[4];
        if (backlashY <= 0)
          backlashY = 0.025f;

        // Set relaxation in the direction of backlash
        if (stageX) {
          smi.backX = (stageX > 0. ? backlashX : -backlashX);
          smi.relaxX = (smi.backX > 0. ? backlashY : -backlashY);
        }
        if (stageY) {
          smi.backY = (stageY > 0. ? backlashX : -backlashX);
          smi.relaxY = (smi.backY > 0. ? backlashY : -backlashY);
        }
        }
      } else {

        // Absolute move: set to values; set Z axis bit if entered
        smi.x = stageX;
        smi.y = stageY;
        smi.z = stageZ;

        smi.axisBits |= (axisX | axisY);
        if (!mItemEmpty[3] && !CMD_IS(STAGETOLASTMULTIHOLE))
          smi.axisBits |= axisZ;
      }

      // Start the movement
      if (smi.axisBits) {
        if (!mScope->MoveStage(smi, truth && backlashX != 0., smi.useSpeed, 0,
          truth))
          SUSPEND_NOLINE("because of failure to start stage movement");
        mMovedStage = true;
      }
    }
  return 0;
}

// RelaxStage
int CMacCmd::RelaxStage(void)
{
  double delX = mItemEmpty[1] ? mScope->GetStageRelaxation() : mItemDbl[1];
  int err = DoStageRelaxation(delX);
  if (err < 0)
    SUSPEND_NOLINE("because of failure to get stage position");
  if (!err) {
    mMovedStage = true;
  } else {
    mWinApp->AppendToLog("Stage is not in known backlash state so RelaxStage cannot "
      "be done", mLogAction);
  }
  return 0;
}

// BackgroundTilt, StopBackgroundTilt
int CMacCmd::BackgroundTilt(void)
{
  double delX;
  StageMoveInfo smi;
  int background = 1;
  if (CMD_IS(STOPBACKGROUNDTILT)) {
    if (!mScope->StageBusy())
      return 0;
    background = -1;
    if (mItemEmpty[1])
      mItemDbl[1] = 0.;
    mItemDbl[1] += mScope->GetTiltAngle();
  }

  if (background > 0 && mScope->StageBusy() > 0)
    mLastIndex = mCurrentIndex;
  else {
    smi.alpha = mItemDbl[1];
    delX = 1.;
    smi.useSpeed = false;
    if (!mItemEmpty[2]) {
      smi.speed = mItemDbl[2];
      if (smi.speed <= 0.)
        ABORT_LINE("Speed entry must be positive in line:/n/n");
      smi.useSpeed = true;
    }
    smi.axisBits = axisA;
    if (!mScope->MoveStage(smi, false, smi.useSpeed, background)) {
      AbortMacro();
      return 1;
    }
  }
  return 0;
}

// ReportStageXYZ
int CMacCmd::ReportStageXYZ(void)
{
  double stageX, stageY, stageZ;

  mScope->GetStagePosition(stageX, stageY, stageZ);
  mLogRpt.Format("Stage %.2f %.2f %.2f", stageX, stageY, stageZ);
  SetRepValsAndVars(1, stageX, stageY, stageZ);
  return 0;
}

// ReportTiltAngle
int CMacCmd::ReportTiltAngle(void)
{
  double delX;

  delX = mScope->GetTiltAngle();
  mLogRpt.Format("%.2f degrees", delX);
  SetRepValsAndVars(1, delX);
  return 0;
}

// ReportStageBusy
int CMacCmd::ReportStageBusy(void)
{
  int index;

  index = mScope->StageBusy();
  mLogRpt.Format("Stage is %s", index > 0 ? "BUSY" : "NOT busy");
  SetRepValsAndVars(1, index > 0 ? 1 : 0);
  return 0;
}

// ReportStageBAxis
int CMacCmd::ReportStageBAxis(void)
{
  double delX;

  delX = mScope->GetStageBAxis();
  mLogRpt.Format("B axis = %.2f degrees", delX);
  SetRepValsAndVars(1, delX);
  return 0;
}

// ReportMag
int CMacCmd::ReportMag(void)
{
  int index, index2, mode = 0;

  index2 = mScope->GetMagIndex();
  if (mWinApp->GetEFTEMMode())
    mode = 1;
  if (mWinApp->GetSTEMMode())
    mode = 2;

  // This is not right if the screen is down and FEI is not in EFTEM
  index = MagOrEFTEMmag(mWinApp->GetEFTEMMode(), index2, mScope->GetSTEMmode());
  mLogRpt.Format("Mag is %d%s%s", index,
    index2 < mScope->GetLowestNonLMmag() ? "   (Low mag)":"",
    B3DCHOICE(mode, mode > 1 ? "   STEM" : "   EFTEM", ""));
  SetRepValsAndVars(1, (double)index,
    index2 < mScope->GetLowestNonLMmag() ? 1. : 0., mode);
  return 0;
}

// ReportMagIndex
int CMacCmd::ReportMagIndex(void)
{
  int index;

  index = mScope->GetMagIndex();
  mLogRpt.Format("Mag index is %d%s", index,
    index < mScope->GetLowestNonLMmag() ? "   (Low mag)":"");
  SetRepValsAndVars(1, (double)index,
    index < mScope->GetLowestNonLMmag() ? 1. : 0.);
  return 0;
}

// ReportCameraLength
int CMacCmd::ReportCameraLength(void)
{
  double delX;

  delX = 0;
  if (!mScope->GetMagIndex())
    delX = mScope->GetLastCameraLength();
  mLogRpt.Format("%s %g%s", delX ? "Camera length is " : "Not in diffraction mode - (",
    delX, delX ? " m" : ")");
  SetRepValsAndVars(1,  delX);
  return 0;
}

// ReportDefocus
int CMacCmd::ReportDefocus(void)
{
  double delX;

  delX = mScope->GetDefocus();
  mLogRpt.Format("Defocus = %.3f um", delX);
  SetRepValsAndVars(1, delX);
  return 0;
}

// ReportFocus, ReportAbsoluteFocus
int CMacCmd::ReportFocus(void)
{
  double delX;

  delX = mScope->GetFocus();
  mLogRpt.Format("Absolute focus = %.5f", delX);
  SetRepValsAndVars(1, delX);
  return 0;
}

// ReportPercentC2
int CMacCmd::ReportPercentC2(void)
{
  double delX, delY;

  delY = mScope->GetIntensity();
  delX = mScope->GetC2Percent(mScope->FastSpotSize(), delY);
  mLogRpt.Format("%s = %.3f%s  -  %.5f", mScope->GetC2Name(), delX, mScope->GetC2Units(),
    delY);
  SetRepValsAndVars(1, delX, delY);
  return 0;
}

// ReportCrossoverPercentC2
int CMacCmd::ReportCrossoverPercentC2(void)
{
  double delX, delY;
  int index;

  index = mScope->GetSpotSize();
  delY = mScope->GetCrossover(index); // probe mode not required, uses current mode
  delX = mScope->GetC2Percent(index, delY);
  mLogRpt.Format("Crossover %s at current conditions = %.3f%s  -  %.5f",
    mScope->GetC2Name(), delX, mScope->GetC2Units(), delY);
  SetRepValsAndVars(1, delX, delY);
  return 0;
}

// ReportIlluminatedArea
int CMacCmd::ReportIlluminatedArea(void)
{
  double delX;

  delX = mScope->GetIlluminatedArea();
  mLogRpt.Format("IlluminatedArea %f", delX);
  SetRepValsAndVars(1, delX);
  return 0;
}

// ReportImageDistanceOffset
int CMacCmd::ReportImageDistanceOffset(void)
{
  double delX;

  delX = mScope->GetImageDistanceOffset();
  mLogRpt.Format("Image distance offset %f", delX);
  SetRepValsAndVars(1, delX);
  return 0;
}

// ReportAlpha
int CMacCmd::ReportAlpha(void)
{
  int index;

  index = mScope->GetAlpha() + 1;
  mLogRpt.Format("Alpha = %d", index);
  SetRepValsAndVars(1, (double)index);
  return 0;
}

// ReportSpotSize
int CMacCmd::ReportSpotSize(void)
{
  int index;

  index = mScope->GetSpotSize();
  mLogRpt.Format("Spot size is %d", index);
  SetRepValsAndVars(1, (double)index);
  return 0;
}

// ReportProbeMode
int CMacCmd::ReportProbeMode(void)
{
  int index;

  index = mScope->ReadProbeMode();
  mLogRpt.Format("Probe mode is %s", index ? "microprobe" : "nanoprobe");
  SetRepValsAndVars(1, (double)index);
  return 0;
}

// ReportEnergyFilter
int CMacCmd::ReportEnergyFilter(void)
{
  BOOL truth;
  double delX;

  if (!mWinApp->ScopeHasFilter())
    ABORT_NOLINE("You cannot use ReportEnergyFilter; there is no filter");
  if (mScope->GetHasOmegaFilter())
    mScope->updateEFTEMSpectroscopy(truth);
  else if (mCamera->CheckFilterSettings())
    ABORT_NOLINE("An error occurred checking filter settings");
  delX = mFiltParam->zeroLoss ? 0. : mFiltParam->energyLoss;
  mLogRpt.Format("Filter slit width %.1f, energy loss %.1f, slit %s",
    mFiltParam->slitWidth, delX, mFiltParam->slitIn ? "IN" : "OUT");
  SetRepValsAndVars(1, mFiltParam->slitWidth, delX,
    mFiltParam->slitIn ? 1. : 0.);
  return 0;
}

// ReportColumnMode
int CMacCmd::ReportColumnMode(void)
{
  int index, index2;

  if (!mScope->GetColumnMode(index, index2)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Column mode %d (%X), submode %d (%X)", index, index, index2,index2);
  SetRepValsAndVars(1, (double)index, (double)index2);
  return 0;
}

// ReportLens
int CMacCmd::ReportLens(void)
{
  CString report;
  double delX;

  JustStripItems(mStrLine, 1, report);
  if (!mScope->GetLensByName(report, delX)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Lens %s = %f", (LPCTSTR)report, delX);
  SetReportedValues(delX);
  return 0;
}

// ReportCoil
int CMacCmd::ReportCoil(void)
{
  double delX, delY;

  if (!mScope->GetDeflectorByName(mStrItems[1], delX, delY)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Coil %s = %f  %f", (LPCTSTR)mStrItems[1], delX, delY);
  SetRepValsAndVars(2, delX, delY);
  return 0;
}

// SetFreeLensControl
int CMacCmd::SetFreeLensControl(void)
{
  if (!mScope->SetFreeLensControl(mItemInt[1], mItemInt[2]))
    ABORT_LINE("Error trying to run:\n\n");
  return 0;
}

// SetLensWithFLC
int CMacCmd::SetLensWithFLC(void)
{
  if (!mScope->SetLensWithFLC(mItemInt[1], mItemDbl[2], mItemInt[3] != 0))
    ABORT_LINE("Error trying to run:\n\n");
  return 0;
}

// ReportLensFLCStatus
int CMacCmd::ReportLensFLCStatus(void)
{
  double delX;
  int index;

  if (!mScope->GetLensFLCStatus(mItemInt[1], index, delX))
    ABORT_LINE("Error trying to run:\n\n");
  mLogRpt.Format("Lens %d, FLC %s  value  %f", mItemInt[1], index ? "ON" : "OFF", delX);
  SetRepValsAndVars(2, index, delX);
  return 0;
}

// SetJeolSTEMflags
int CMacCmd::SetJeolSTEMflags(void)
{
  if (mItemInt[1] < 0 || mItemInt[1] > 0xFFFFFF || mItemInt[2] < 0 || mItemInt[2] > 15)
      ABORT_LINE("Entries must fit in 24 and 4 bits in: \n\n");
  mCamera->SetBaseJeolSTEMflags(mItemInt[1] + (mItemInt[2] << 24));

  return 0;
}

// GetSTEMBrightContrast, SetSTEMBrightContrast
int CMacCmd::GetSTEMBrightContrast(void)
{
  bool setIt = CMD_IS(SETSTEMBRIGHTCONTRAST);
  int index;
  double bright, contrast;
  if (!mCamParams->STEMcamera || !mCamParams->FEItype)
    ABORT_LINE("The currently selected camera must be the FEI STEM for line:\n\n");
  SubstituteLineStripItems(mStrLine, setIt ? 3 : 1, mStrCopy);
  for (index = 0; index < mCamParams->numChannels; index++)
    if (mStrCopy.CompareNoCase(mCamParams->channelName[index]))
      break;
  if (index == mCamParams->numChannels)
    ABORT_LINE("There is no detector with this \"ChannelName\" in the SerialEM camera"
      " properties for line:\n\n");
  if ((!setIt && !mScope->GetSTEMBrightnessContrast((LPCTSTR)mStrCopy, bright, contrast))
    || (setIt && !mScope->SetSTEMBrightnessContrast((LPCTSTR)mStrCopy, mItemDbl[1], 
      mItemDbl[2]))) {
    AbortMacro();
    return 1;
  }
  if (!setIt)
    SetReportedValues(bright, contrast);
  return 0;
}

// SetCameraPLAOffset
int CMacCmd::SetCameraPLAOffset(void)
{
  double delISX, delISY;
  float shiftX, shiftY, floatX, floatY;

  if (!mImBufs[0].mImage || !mImBufs[1].mImage)
    ABORT_NOLINE("There must be images in both buffers A and B to refine detector "
      "offset");
  if (!mScope->GetUsePLforIS() || !JEOLscope)
    ABORT_NOLINE("SetCameraPLAOffset can be used only on a JEOL scope using"
      " PLA for image shift");
  mScope->GetImageShift(delISX, delISY);
  mScope->GetDetectorOffsets(floatX, floatY);
  shiftX = floatX + (float)(delISX - mImBufs[1].mISX);
  shiftY = floatY + (float)(delISY - mImBufs[1].mISY);
  PrintfToLog("Camera offset changed from %.3f  %.3f to %.3f %.3f", floatX, floatY,
    shiftX, shiftY);
  mCamera->SetCameraISOffset(mWinApp->GetCurrentCamera(), shiftX, shiftY);
  mScope->SetDetectorOffsets(shiftX, shiftY);
  mScope->SetImageShift(delISX, delISY);
  return 0;
}

// RemoveAperture, ReInsertAperture
int CMacCmd::RemoveAperture(void)
{
  int index, index2;

  index = mItemInt[1];
  if (mStrItems[1] == "CL")
    index = 1;
  if (mStrItems[1] == "OL")
    index = 2;
  if (CMD_IS(REINSERTAPERTURE))
    index2 = mScope->ReInsertAperture(index);
  else
    index2 = mScope->RemoveAperture(index);
  if (index2)
    ABORT_LINE("Script aborted due to error starting aperture movement in:\n\n");
  mMovedAperture = true;
  return 0;
}

// ReportApertureSize
int CMacCmd::ReportApertureSize(void)
{
  int size = mScope->GetApertureSize(mItemInt[1]);
  if (size < 0) {
    AbortMacro();
    return 1;
  }
  if (size < 2)
    mLogRpt.Format("Aperture %d is %s", mItemInt[1], !size ? "retracted" : "disabled");
  else if (size < 8) 
    mLogRpt.Format("Aperture %d is phase plate in position %d", mItemInt[1], size - 1);
  else
    mLogRpt.Format("Size of aperture %d is %d um", mItemInt[1], size);
  SetRepValsAndVars(2, size);
  return 0;
}

// SetApertureSize
int CMacCmd::SetApertureSize(void)
{
  if (!mScope->SetApertureSize(mItemInt[1], mItemInt[2])) {
    AbortMacro();
    return 1;
  }
  mMovedAperture = true;
  return 0;
}

// PhasePlateToNextPos
int CMacCmd::PhasePlateToNextPos(void)
{
  if (!mScope->MovePhasePlateToNextPos())
    ABORT_LINE("Script aborted due to error starting phase plate movement in:\n\n");
  mMovedAperture = true;
  return 0;
}

// ReportPhasePlatePos
int CMacCmd::ReportPhasePlatePos(void)
{
  int index;

  index = mScope->GetCurrentPhasePlatePos();
  if (index < 0)
    ABORT_LINE("Script aborted due to error in:\n\n");
  mLogRpt.Format("Current phase plate position is %d", index + 1);
  SetRepValsAndVars(1, index + 1.);
  return 0;
}

// ReportMeanCounts
int CMacCmd::ReportMeanCounts(void)
{
  CString report;
  EMimageBuffer *imBuf;
  KImage *image;
  double delX;
  int index, sizeX, sizeY;
  float bmin, bmax, bmean, bSD;

  if (ConvertBufferLetter(mStrItems[1], 0, true, index, report))
    ABORT_LINE(report);
  imBuf = &mImBufs[index];
  if (mItemEmpty[2] || mItemInt[2] < 2) {
    delX = mProcessImage->WholeImageMean(imBuf);
    if (mStrItems[2] == "1" && imBuf->mBinning && imBuf->mExposure > 0.) {
      delX /= imBuf->mBinning * imBuf->mBinning * imBuf->mExposure *
        mWinApp->GetGainFactor(imBuf->mCamera, imBuf->mBinning);
      mLogRpt.Format("Mean of buffer %s = %.2f unbinned counts/sec", mStrItems[1], delX);
    } else
      mLogRpt.Format("Mean of buffer %s = %.1f", mStrItems[1], delX);
    SetReportedValues(delX);
  } else {
    image = mImBufs[index].mImage;
    image->getSize(sizeX, sizeY);
    ProcMinMaxMeanSD(image->getData(), image->getType(), sizeX, sizeY, 0,
      sizeX - 1, 0, sizeY - 1, &bmean, &bmin, &bmax, &bSD);
    mLogRpt.Format("Buffer %s: mean = %.1f  sd = %.2f  min = %.1f  max = %.1f",
      mStrItems[1], bmean, bSD, bmin, bmax);
    SetReportedValues(bmean, bSD, bmin, bmax);
  }
  return 0;
}

// ReportFileZsize
int CMacCmd::ReportFileZsize(void)
{
  int index;
  MontParam *param = mWinApp->GetMontParam();

  if (!mWinApp->mStoreMRC)
    SUSPEND_LINE("because there is no open image file to report for line: \n\n");
  if (mBufferManager->CheckAsyncSaving())
    SUSPEND_NOLINE("because of file write error");
  index = mWinApp->mStoreMRC->getDepth();
  mLogRpt.Format("Z size of file = %d", index);
  if (mWinApp->Montaging()) {
    mStrCopy.Format("   # of montage sections = %d", param->zCurrent);
    SetRepValsAndVars(1, (double)index, param->zCurrent);
    mLogRpt += mStrCopy;
  } else
    SetRepValsAndVars(1, (double)index);
  return 0;
}

// SubareaMean
int CMacCmd::SubareaMean(void)
{
  double delX;
  int ix0, ix1, iy0, iy1, sizeX, sizeY;

  ix0 = mItemInt[1];
  ix1 = mItemInt[2];
  iy0 = mItemInt[3];
  iy1 = mItemInt[4];
  if (mImBufs->mImage)
    mImBufs->mImage->getSize(sizeX, sizeY);
  if (!mImBufs->mImage || mItemEmpty[4] || ix0 < 0 || ix1 >= sizeX || ix1 < ix0
    || iy0 < 0 || iy1 >= sizeY || iy1 < iy0)
      ABORT_LINE("Not enough coordinates, coordinates out of bounds, or no image "
        "in A in statement: \n\n");
  delX = ProcImageMean(mImBufs->mImage->getData(), mImBufs->mImage->getType(),
    sizeX, sizeY, ix0, ix1, iy0, iy1);
  mLogRpt.Format("Mean of subarea (%d, %d) to (%d, %d) = %.2f", ix0, iy0, ix1, iy1,
    delX);
  SetRepValsAndVars(5, delX);
  return 0;
}

// CircularSubareaMean
int CMacCmd::CircularSubareaMean(void)
{
  double delX;
  int i, ix0, iy0, sizeX, sizeY;

  ix0 = mItemInt[1];
  iy0 = mItemInt[2];
  i = mItemInt[3];
  if (mImBufs->mImage)
    mImBufs->mImage->getSize(sizeX, sizeY);
  if (!mImBufs->mImage || mItemEmpty[3] || ix0 - i < 0 || iy0 - i < 0 ||
    ix0 + i >= sizeX || iy0 + i >= sizeY)
    ABORT_LINE("Not enough coordinates, coordinates out of bounds, or no image "
      "in A in statement: \n\n");
  delX = ProcImageMeanCircle(mImBufs->mImage->getData(), mImBufs->mImage->getType(),
    sizeX, sizeY, ix0, iy0, i);
  mLogRpt.Format("Mean of subarea with radius %d centered around (%d, %d) = %.2f", i,
    ix0, iy0, delX);
  SetRepValsAndVars(4, delX);
  return 0;
}

// ElectronStats, RawElectronStats
int CMacCmd::ElectronStats(void)
{
  CString report;
  KImage *image;
  double delX;
  int index, sizeX, sizeY;
  float backlashX, backlashY, bmin, bmax, bmean, bSD, cpe, shiftX;

  if (ConvertBufferLetter(mStrItems[1], 0, true, index, report))
    ABORT_LINE(report);
  delX = mImBufs[index].mBinning;
  if (mImBufs[index].mCamera < 0 || !delX || !mImBufs[index].mExposure)
    ABORT_LINE("Image buffer does not have enough information for dose statistics in:"
    "\r\n\r\n");
  cpe = mProcessImage->CountsPerElectronForImBuf(&mImBufs[index]);
  if (!cpe)
    ABORT_LINE("Camera does not have a CountsPerElectron property for:\r\n\r\n");
  image = mImBufs[index].mImage;
  image->getSize(sizeX, sizeY);
  ProcMinMaxMeanSD(image->getData(), image->getType(), sizeX, sizeY, 0,
    sizeX - 1, 0, sizeY - 1, &bmean, &bmin, &bmax, &bSD);
  mCamParams = mWinApp->GetCamParams() + mImBufs[index].mCamera;
  if (CamHasDoubledBinnings(mCamParams))
    delX /= 2;
  bmin /= cpe;
  bmax /= cpe;
  bmean /= cpe;
  bSD /= cpe;
  backlashX = (float)(bmean / (mImBufs[index].mExposure * delX * delX));
  backlashY = backlashX;
  if (CMD_IS(ELECTRONSTATS)) {
    if (mImBufs[index].mK2ReadMode > 0)
      backlashY = mProcessImage->LinearizedDoseRate(mImBufs[index].mCamera,
        backlashX);
    if (mImBufs[index].mDoseRatePerUBPix > 0.) {
      SEMTrace('1', "Dose rate computed from mean %.3f  returned from DM %.3f",
        backlashY, mImBufs[index].mDoseRatePerUBPix);
      backlashY = mImBufs[index].mDoseRatePerUBPix;
    }
  }
  shiftX = backlashY / backlashX;
  mLogRpt.Format("Min = %.3f  max = %.3f  mean = %.3f  SD = %.3f electrons/pixel; "
    "dose rate = %.3f e/physical pixel/sec", bmin * shiftX, bmax * shiftX,
    bmean *shiftX, bSD * shiftX, backlashY);
  SetRepValsAndVars(2, bmin * shiftX, bmax * shiftX, bmean * shiftX,
    bSD *shiftX, backlashY);
  return 0;
}

// CalibrateElectronDose
int CMacCmd::CalibrateElectronDose(void)
{
  if (mWinApp->mBeamAssessor->CalibrateElectronDose(false))
    ABORT_LINE("One of the preconditions for calibrating dose is not met for line:\n\n");
  return 0;
}

// AccumulateRecordDose
int CMacCmd::AccumulateRecordDose(void)
{

  // Disable it, or initialize it if it is disabled, or add to it
  if (mItemFlt[1] < 0.)
    mCumulRecordDose = -1.;
  else if (mCumulRecordDose < 0.)
    mCumulRecordDose = mItemFlt[1];
  else
    mCumulRecordDose += mItemFlt[1];
  return 0;
}

// CropImage, CropCenterToSize
int CMacCmd::CropImage(void)
{
  CString report;
  BOOL truth;
  EMimageBuffer *imBuf;
  int index, ix0, ix1, iy0, iy1, sizeX, sizeY;
  float backlashX, backlashY;

  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report, true))
    ABORT_LINE(report);
  imBuf = ImBufForIndex(index);
  imBuf->mImage->getSize(sizeX, sizeY);
  imBuf->mImage->getShifts(backlashX, backlashY);
  truth = CMD_IS(CROPIMAGE);
  if (truth) {
    ix0 = mItemInt[2];
    ix1 = mItemInt[3];
    iy0 = mItemInt[4];
    iy1 = mItemInt[5];

    // Test for whether centered:if not, it needs to be marked as processed
    truth = B3DABS(ix0 + ix1 + 1 - sizeX) > 1 || B3DABS(iy0 + iy1 + 1 - sizeY) > 1;
  } else {
    if (mItemInt[2] > sizeX || mItemInt[3] > sizeY)
      ABORT_LINE("Image is already smaller than size requested in:\n\n");
    if (mItemInt[2] <= 0 || mItemInt[3] <= 0)
      ABORT_LINE("Size to crop to must be positive in:\n\n");
    ix0 = (sizeX - mItemInt[2]) / 2;
    iy0 = (sizeY - mItemInt[3]) / 2;
    ix1 = ix0 + mItemInt[2] - 1;
    iy1 = iy0 + mItemInt[3] - 1;
  }
  ix0 = mProcessImage->CropImage(imBuf, iy0, ix0, iy1, ix1);
  if (ix0) {
    report.Format("Error # %d attempting to crop image in buffer %c in statement: \n\n"
      , ix0, mStrItems[1].GetAt(0));
    ABORT_LINE(report);
  }

  // Mark as cropped if centered and unshifted: this allows autoalign to apply image
  // shift on scope
  mImBufs->mCaptured = (truth || backlashX || backlashY) ? BUFFER_PROCESSED :
    BUFFER_CROPPED;
  return 0;
}

// ReduceImage
int CMacCmd::ReduceImage(void)
{
  CString report;
  int index, ix0;

  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report, true))
    ABORT_LINE(report);
  ix0 = mProcessImage->ReduceImage(ImBufForIndex(index), mItemFlt[2], &report);
  if (ix0) {
    report += " in statement:\n\n";
    ABORT_LINE(report);
  }
  return 0;
}

// Rotate90CW
int CMacCmd::Rotate90CW(void)
{
  CString report;
  int index;

  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report))
    ABORT_LINE(report);
  mWinApp->mMainView->SetImBufIndex(index);
  mProcessImage->RotateImage(FALSE);
  return 0;
}

// Rotate90CCW
int CMacCmd::Rotate90CCW(void)
{
  CString report;
  int index;

  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report))
    ABORT_LINE(report);
  mWinApp->mMainView->SetImBufIndex(index);
  mProcessImage->RotateImage(TRUE);
  return 0;
}

// FFT
int CMacCmd::FFT(void)
{
  CString report;
  int index, index2;

  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report))
    ABORT_LINE(report);
  index2 = 1;
  if (!mItemEmpty[2])
    index2 = mItemInt[2];
  if (index2 < 1 || index2 > 8)
    ABORT_LINE("Binning must be between 1 and 8 in line:\n\n");
  mProcessImage->GetFFT(&mImBufs[index], index2, BUFFER_FFT);
  return 0;
}

// FilterImage
int CMacCmd::FilterImage(void)
{
  CString report;
  int index, index2;

  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report, true) ||
    ConvertBufferLetter(mStrItems[6], 0, false, index2, report))
    ABORT_LINE(report);
  if (mProcessImage->FilterImage(ImBufForIndex(index), index2, mItemFlt[2], mItemFlt[5],
    mItemFlt[3], mItemFlt[4]))
    ABORT_LINE("Failed to filter image for line:\n\n");
  return 0;
}

// AddImages, SubtractImages, MultiplyImages, DivideImages, ComputeIceThickness
int CMacCmd::CombineImages(void)
{
  CString report;
  int index, index2, proc, outInd;

  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report) ||
    ConvertBufferLetter(mStrItems[2], -1, true, index2, report) ||
    ConvertBufferLetter(mStrItems[3], 0, false, outInd, report))
    ABORT_LINE(report);
  proc = PROC_ADD_IMAGES;
  if (CMD_IS(SUBTRACTIMAGES))
    proc = PROC_SUBTRACT_IMAGES;
  if (CMD_IS(MULTIPLYIMAGES))
    proc = PROC_MULTIPLY_IMAGES;
  if (CMD_IS(DIVIDEIMAGES))
    proc = PROC_DIVIDE_IMAGES;
  if (CMD_IS(COMPUTEICETHICKNESS)) {
    proc = PROC_COMPUTE_THICKNESS;
    if (!mItemEmpty[4]) {
      if (mItemFlt[4] <= 0.)
        ABORT_LINE("Thickness coefficient is not positive in line:\n\n");
      mProcessImage->SetNextThicknessCoeff(mItemFlt[4]);
    } else if (mProcessImage->GetThicknessCoefficient() <= 0.)
      ABORT_LINE("No thickness coefficient is set in properties for line:\n\n");
  }
  if (mProcessImage->CombineImages(index, index2, outInd, proc))
    ABORT_LINE("Failed to combine images for line:\n\n");
  return 0;
}

// ScaleImage
int CMacCmd::ScaleImage(void)
{
  CString report;
  int index, index2;

  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report, true) ||
    ConvertBufferLetter(mStrItems[4], 0, false, index2, report))
    ABORT_LINE(report);
  if (mProcessImage->ScaleImage(ImBufForIndex(index), index2, mItemFlt[2], mItemFlt[3],
    !mItemEmpty[5] && mItemInt[5]))
    ABORT_LINE("Failed to scale image for line:\n\n");
  return 0;
}

// ConvertToBytes
int CMacCmd::ConvertToBytes(void)
{
  CString report;
  int index;
  EMimageBuffer *imBuf;
  if (!mItemEmpty[2] && mItemEmpty[3])
    ABORT_LINE("You must enter two scaling values or none for:\n\n");
  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report, true))
    ABORT_LINE(report);
  imBuf = ImBufForIndex(index);
  if (imBuf->mImage->getType() == kRGB)
    ABORT_LINE("The buffer has a color image that cannot be converted in:\n\n");
  if (imBuf->mImage->getType() == kUBYTE)
    mLogRpt = "The image to be converted is already bytes";
  else
    imBuf->ConvertToByte(mItemEmpty[2] ? 0.f : mItemFlt[2], 
      mItemEmpty[3] ? 0.f : mItemFlt[3]);
  if (index >= 0)
    mWinApp->SetCurrentBuffer(index);
  return 0;
}

// PasteImagesTogether
int CMacCmd::PasteImagesTogether(void)
{
  CString report;
  int index, index2, outInd;
  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report, true) ||
    ConvertBufferLetter(mStrItems[2], -1, true, index2, report, true) ||
    ConvertBufferLetter(mStrItems[3], 0, false, outInd, report))
    ABORT_LINE(report);
  if (mProcessImage->PasteImages(ImBufForIndex(index), ImBufForIndex(index2), outInd,
    !mItemEmpty[4] && mItemInt[4])) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// ScaledSpectrum, SpectrumBesideImage
int CMacCmd::ScaledSpectrum(void)
{
  CString report;
  int index, err, nx, ny, maxSize, finalSize, padSize;
  KImage *image;
  EMimageBuffer tmpImBuf;
  float factor;
  unsigned char *buf;
  bool sideBySide = CMD_IS(SPECTRUMBESIDEIMAGE);
  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report, false))
    ABORT_LINE(report);
  image = mImBufs[index].mImage;
  image->getSize(nx, ny);
  maxSize = B3DMAX(nx, ny);
  padSize = XCorrNiceFrame(4 * ((maxSize + 3) / 4), 4, 19);
  finalSize = mItemInt[2] ? mItemInt[2] : padSize;
  finalSize = 4 * ((finalSize + 3) / 4);
  factor = B3DMAX(1.f, (float)ny / (float)finalSize);
  if (sideBySide) {
    finalSize = B3DMIN(finalSize, padSize);
    if (finalSize < 100)
      ABORT_LINE("The output size needs to eb either 0 or at least 100for:\n\n");
  } else if (finalSize < 100 || finalSize > padSize)
    ABORT_LINE("The output size needs to be either 0 or between 100 and the maximum"
      " dimension of the input image for:\n\n");
  NewArray2(buf, unsigned char, finalSize, finalSize);
  if (!buf)
    ABORT_LINE("Failed to allocate array for output of:\n\n");

  image->Lock();
  err = spectrumScaled(image->getData(), image->getType(), nx, ny, buf, padSize,
    finalSize, mWinApp->GetBkgdGrayOfFFT(), mWinApp->GetTruncDiamOfFFT(), 4, twoDfft);
  image->UnLock();
  if (err) {
    delete[] buf;
    report.Format("Error %d calling spectrum with finalSize %d for:\n\n", err, finalSize);
    ABORT_LINE(report);
  }
  if (sideBySide) {
    tmpImBuf.mImage = new KImage();
    tmpImBuf.mImage->useData((char *)buf, finalSize, finalSize);
    if (factor > 1.) {

      // Change factor to make reduction gave a factor of 4 in X, needed to avoid gray
      // line when it converts to bytes by assigning the pixmap
      err = 2 * (int)(0.5 * nx / factor);
      err = 4 * (err / 4);
      factor = (float)nx / (float)err - 0.0001f;
      if (mProcessImage->ReduceImage(&mImBufs[index], factor, &report))
        ABORT_LINE(report + " for:\n\n");
      index = 0;
    }
    mProcessImage->PasteImages(&mImBufs[index], &tmpImBuf, 0, false);
  } else 
    mProcessImage->NewProcessedImage(&mImBufs[index], (short *)buf, kUBYTE, finalSize,
      finalSize, 1);
  return 0;
}

// CtfFind
int CMacCmd::CtfFind(void)
{
  CString report;
  double delX, delY;
  int index;

  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report))
    ABORT_LINE(report);
  CtffindParams param;
  float resultsArray[7];
  if (mProcessImage->InitializeCtffindParams(&mImBufs[index], param))
    ABORT_LINE("Error initializing Ctffind parameters for line:\n\n");
  if (mItemDbl[2] > 0. || mItemDbl[3] > 0. || mItemDbl[2] < -100 || mItemDbl[3] < -100)
    ABORT_LINE("Minimum and maximum defocus must be negative and in microns");
  if (mItemDbl[2] < mItemDbl[3]) {
    delX = mItemDbl[2];
    mItemDbl[2] = mItemDbl[3];
    mItemDbl[3] = delX;
  }
  param.minimum_defocus = -(float)(10000. * mItemDbl[2]);
  param.maximum_defocus = -(float)(10000. * mItemDbl[3]);
  param.slower_search = mItemEmpty[4] || mItemInt[4] == 0;
  if (mItemInt[5] != 0) {
    if (mItemInt[5] < 128 || mItemInt[5] > 640)
      ABORT_LINE("The box size must be between 128 and 640 in the line:\n\n");
    param.box_size = mItemInt[5];
  }

  // Phase entries.  Convert all from degrees to radians.  The assumed phase shift is
  // in radians and was being converted to degrees and passed that way (12/20/18)
  if (!mItemEmpty[6]) {
    delX = mItemDbl[6] * DTOR;
    if (delX == 0)
      delX = mProcessImage->GetPlatePhase();
    param.minimum_additional_phase_shift = param.maximum_additional_phase_shift =
      (float)(delX);
    param.find_additional_phase_shift = true;
    if (!mItemEmpty[7] && (mItemDbl[6] != 0 || mItemDbl[7] != 0)) {
      delY = mItemDbl[7] * DTOR;
      if (delY < delX)
        ABORT_LINE("The maximum phase shift is less than the minimum in line:\n\n");
      if (delY - delX > 125 * DTOR)
        ABORT_LINE("The range of phase shift to search is more than 125 degrees in "
        "line:\n\n");
      param.maximum_additional_phase_shift = (float)delY;
    }
    if (!mItemEmpty[8] && mItemDbl[8])
      param.additional_phase_shift_search_step = (float)(mItemDbl[8] * DTOR);
    if (mItemInt[9]) {
      param.astigmatism_is_known = true;
      param.known_astigmatism = 0.;
      param.known_astigmatism_angle = 0.;
    }
  }
  mProcessImage->SetCtffindParamsForDefocus(param,
    0.8 *mItemDbl[2] + 0.2 * mItemDbl[3], true);
  param.compute_extra_stats = true;
  if (mProcessImage->RunCtffind(&mImBufs[index], param, resultsArray, 
    mLogAction == LOG_IGNORE))
    ABORT_LINE("Ctffind fitting returned an error for line:\n\n");
  SetReportedValues(-(resultsArray[0] + resultsArray[1]) / 20000.,
    (resultsArray[0] - resultsArray[1]) / 10000., resultsArray[2],
    resultsArray[3] / DTOR, resultsArray[4], resultsArray[5]);
  return 0;
}

// Ctfplotter
int CMacCmd::Ctfplotter(void)
{
  CString report, command;
  float phase = 0., imPixel, cropPixel = 0., fitStart = -1., fitEnd = 0., tiltOffset = 0.;
  int index, astigPhase = 1, resolOrTune = 1;

  if (ConvertBufferLetter(mStrItems[1], -1, true, index, report))
    ABORT_LINE(report);

  // Need pixel size here, other items from imbuf are checked in called routine
  imPixel = 1000.f * mShiftManager->GetPixelSize(&mImBufs[index]);
  if (!imPixel)
    ABORT_LINE("No pixel size is available for the image for line:\n\n");
  
  if (mItemDbl[2] > 0. || mItemDbl[3] > 0. || mItemDbl[2] < -100 || mItemDbl[3] < -100)
    ABORT_LINE("Minimum and maximum defocus must be negative and in microns");
  if (!mItemEmpty[4])
    astigPhase = mItemInt[4];
  if (!mItemEmpty[5]) {
    phase = mItemFlt[5];
    if (!phase)
      phase = mProcessImage->GetPlatePhase();
  }
  if (!mItemEmpty[6])
    tiltOffset = mItemFlt[6];

  // Handle resolution or tuning entry
  if (!mItemEmpty[7]) {
    resolOrTune = mItemInt[7];
    if (resolOrTune > 1 && resolOrTune < 100)
      ABORT_LINE("Power spectrum resolution must be at least 100 in line:\n\n");
    if (!resolOrTune || resolOrTune > 1) {
      if (mItemEmpty[10])
        ABORT_LINE("If power spectrum resolution is entered or the default specified, "
          "then fitting range must be entered for line:\n\n");
      if (mItemFlt[8] >= imPixel)
        cropPixel = mItemFlt[8];
      fitStart = mItemFlt[9];
      fitEnd = mItemFlt[10];
      if (fitStart > 0.51)
        fitStart = 10.f * imPixel / fitStart;
      if (fitEnd > 0.51)
        fitEnd = 10.f * imPixel / fitEnd;
      if (fitStart > fitEnd || (fitStart >= 0 && fitStart < 0.1)) {
        report.Format("The start of the fitting range (%.3f/pixel) is too small or past "
          "the end of the range (%.2f/pixel) in line:\n\n", fitStart, fitEnd);
        ABORT_LINE(report);
      }
      if (fitEnd > 0.48) {
        report.Format("The end of the fitting range (%.3f/pixel) is too high in line:\n\n"
          , fitEnd);
        ABORT_LINE(report);
      }
    }
    if ((resolOrTune < 0 || resolOrTune == 1) && !mItemEmpty[8])
      ABORT_LINE("You cannot enter crop or fitting values after specifying to use last"
        " parameters or to autotune for line:\n\n");
    if (resolOrTune < 0 && !mWinApp->mExternalTools->GetLastCropPixel())
      ABORT_LINE("There are no previous fitting and sampling parameters to use for "
          "line:\n\n");
  }

  // Save the image to shared memory
  mShrMemFile = mWinApp->mExternalTools->SaveBufferToSharedMemory(index,
    "ctftmp.mrc", report);
  if (!mShrMemFile)
    ABORT_LINE(report + " in line:\n\n");

  // Get the command and run it in a thread, set flags
  if (mWinApp->mExternalTools->MakeCtfplotterCommand(report, index, tiltOffset,
    mItemFlt[2], mItemFlt[3], astigPhase, phase, resolOrTune, cropPixel, fitStart, 
    fitEnd, mEnteredName))
    ABORT_LINE(report + " in line:\n\n");
  if (mWinApp->mExternalTools->RunCreateProcess(mEnteredName, report, true, CString(""))) {
    AbortMacro();
    return 1;
  }
  mRanExtProcess = true;
  mRanCtfplotter = true;
  return 0;
}

// ReportCtplotterTuning
int CMacCmd::ReportCtplotterTuning(void)
{
  int resol = mWinApp->mExternalTools->GetLastPSresol();
  float crop = mWinApp->mExternalTools->GetLastCropPixel();
  float fitStart = mWinApp->mExternalTools->GetLastFitStart();
  float fitEnd = mWinApp->mExternalTools->GetLastFitEnd();
  if (!resol) {
    mLogRpt = "There are no stored values from previous autotuning";
    SetReportedValues(&mStrItems[1], 0);
  } else {
    mLogRpt.Format("Tuned parameters are psResol = %d,  crop to pixel %.3f, fit to %.3f"
      " - %.3f/pixel", resol, crop, fitStart, fitEnd);
    SetReportedValues(&mStrItems[1], resol, crop, fitStart, fitEnd);
  }
  return 0;
}

// GraphValuesInArrays, GraphPreStoredValues
int CMacCmd::GraphValuesInArrays(void)
{
  bool stored = CMD_IS(GRAPHPRESTOREDVALUES);
  bool ifTypes = mItemInt[1] != 0;
  int index = 2, ind, numCol = 0;
  int numToGraph = (int)mGraphColumns.size();
  IntVec dummy;
  Variable *var;
  CString errString;

  if (!numToGraph) {
    if (stored) {
      numToGraph = (int)(mStoredGraphValues.size());
    } else {
      for (ind = index; ind < MAX_MACRO_TOKENS; ind++) {
        if (mItemEmpty[index])
          break;
        numToGraph++;
      }
    }
  }

  if (ifTypes && stored && !mStoredGraphTypes.size())
    ABORT_LINE("No prestored type identifiers were saved for graphing with in line:\n\n");
  if (stored && !mStoredGraphValues.size())
    ABORT_LINE("No prestored values were saved for graphing in line:\n\n");
  if (ifTypes && numToGraph > 2 - mGraphVsOrdinals)
    ABORT_LINE("There are too many columns of values for graphing by type in line:\n\n");
  if (!stored) {
    mStoredGraphTypes.clear();
    mStoredGraphValues.clear();
    for (; index < MAX_MACRO_TOKENS; index++) {
      if (mItemEmpty[index])
        break;
      var = LookupVariable(mStrItems[index], ind);
      if (!var)
        ABORT_LINE("Variable \"" + mStrItems[index] + "\" is not defined for line:\n\n");

      if (ifTypes && index == 2) {
        FillVectorFromArrayVariable(NULL, &mStoredGraphTypes, var);
      } else {
        mStoredGraphValues.resize(numCol + 1);
        FillVectorFromArrayVariable(&mStoredGraphValues[numCol++], NULL, var);
      }
    }
  }
  ind = mWinApp->mExternalTools->StartGraph(mStoredGraphValues, 
    ifTypes ? mStoredGraphTypes : dummy, mGraphTypeList, mGraphColumns, mGraphSymbols, 
    mGraphAxisLabel, mGraphKeys, errString, mConnectGraph,
    mGraphVsOrdinals, mGraphColorOpt, mGraphXlogSqrt, mGraphXlogBase, mGraphYlogSqrt, 
    mGraphYlogBase, mGraphXmin, mGraphXmax, mGraphYmin, mGraphYmax);
  if (ind) {
    if (!errString.IsEmpty())
      ABORT_LINE(errString + " in line\n\n");
    AbortMacro();
  }
  ClearGraphVariables();
  return ind;
}

// SetGraphAxisLabel
int CMacCmd::SetGraphAxisLabel(void)
{
  SubstituteLineStripItems(mStrLine, 1, mGraphAxisLabel);
  return 0;
}

// SetGraphKeyLabel
int CMacCmd::SetGraphKeyLabel(void)
{
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  mGraphKeys.push_back((LPCTSTR)mStrCopy);
  return 0;
}

// SetGraphTypes
int CMacCmd::SetGraphTypes(void)
{
  SetGraphListVec(mGraphTypeList);
  return 0;
}

// SetGraphColumns
int CMacCmd::SetGraphColumns(void)
{
  SetGraphListVec(mGraphColumns);
  return 0;
}

// SetGraphSymbols
int CMacCmd::SetGraphSymbols(void)
{
  SetGraphListVec(mGraphSymbols);
  return 0;
}

// SetGraphOptions
int CMacCmd::SetGraphOptions(void)
{
  int ind;
  for (ind = 1; ind < MAX_MACRO_TOKENS; ind++) {
    if (mItemEmpty[ind])
      break;
    if (mStrItems[ind].Find("con") == 0) {
      mConnectGraph = true;
    } else if (mStrItems[ind].Find("ord") == 0) {
      mGraphVsOrdinals = true;
    } else if (mStrItems[ind].Find("color") == 0) {
      if (mItemEmpty[ind + 1])
        ABORT_LINE("A value must be entered after \"" + mStrItems[ind] + 
          "\" in line:\n\n");
      mGraphColorOpt = mItemInt[++ind];
    } else if (mStrItems[ind] == "xsqrt" || mStrItems[ind] == "xlog") {
      if (mItemEmpty[ind + 1])
        ABORT_LINE("A base value to add must be entered after \"" + mStrItems[ind] +
          "\" in line:\n\n");
      mGraphXlogSqrt = mStrItems[ind++] == "xsqrt" ? 2 : 1;
      mGraphXlogBase = mItemFlt[ind];
    } else if (mStrItems[ind] == "ysqrt" || mStrItems[ind] == "ylog") {
      if (mItemEmpty[ind + 1])
        ABORT_LINE("A base value to add must be entered after \"" + mStrItems[ind] +
          "\" in line:\n\n");
      mGraphYlogSqrt = mStrItems[ind++] == "ysqrt" ? 2 : 1;
      mGraphYlogBase = mItemFlt[ind];
    } else if (mStrItems[ind].Find("xran") == 0 || mStrItems[ind].Find("yran") == 0) {
      if (mItemEmpty[ind + 2])
        ABORT_LINE("Min and max values must be entered after \"" + mStrItems[ind] +
          "\" in line:\n\n");
      if (mStrItems[ind].Find("xran") == 0) {
        mGraphXmin = mItemFlt[ind + 1];
        mGraphXmax = mItemFlt[ind + 2];
      } else {
        mGraphYmin = mItemFlt[ind + 1];
        mGraphYmax = mItemFlt[ind + 2];
      }
      ind += 2;
    } else {
      ABORT_LINE("Unrecognized graphing option \"" + mStrItems[ind] + "\" in line:\n\n");
    }
  }
  return 0;
}

// CloseAllGraphs
int CMacCmd::CloseAllGraphs(void)
{
  mWinApp->mExternalTools->CloseAllGraphs();
  return 0;
}

// GetLastTiltXYZToGraph
int CMacCmd::GetLastTiltXYZToGraph(void)
{
  if (mWinApp->mTSController->StoreXYZForGraphing())
    ABORT_LINE("There are no X/Y/Z values from a tilt series available for line:\n\n");
  return 0;
}

// ImageProperties
int CMacCmd::ImageProperties(void)
{
  CString report;
  EMimageBuffer *imBuf;
  double delISX, delX, delY;
  int index, sizeX, sizeY;

  if (ConvertBufferLetter(mStrItems[1], 0, true, index, report, true))
    ABORT_LINE(report);
  imBuf = ImBufForIndex(index);
  imBuf->mImage->getSize(sizeX, sizeY);
  delX = 1000. * mShiftManager->GetPixelSize(&mImBufs[index]);
  mLogRpt.Format("Image size %d by %d, binning %s, exposure %.4f",
    sizeX, sizeY, (LPCTSTR)imBuf->BinningText(),
    imBuf->mExposure);
  if (delX) {
    mStrCopy.Format(", pixel size " + mWinApp->PixelFormat((float)delX), (float)delX);
    mLogRpt += mStrCopy;
  }
  if (imBuf->mSecNumber < 0) {
    delY = -1;
    mStrCopy = ", read in";
  } else {
    delY = imBuf->mConSetUsed;
    mStrCopy.Format(", %s parameters", mModeNames[imBuf->mConSetUsed]);
  }
  mLogRpt += mStrCopy;
  if (imBuf->mMapID) {
    mStrCopy.Format(", mapID %d", imBuf->mMapID);
    mLogRpt += mStrCopy;
  }
  SetRepValsAndVars(2, (double)sizeX, (double)sizeY,
    imBuf->mBinning / (double)B3DMAX(1, imBuf->mDivideBinToShow),
    (double)imBuf->mExposure, delX, delY);
  delISX = SEMTickInterval(1000. * imBuf->mTimeStamp, mWinApp->ProgramStartTime()) /
    1000.;
  report.Format("%.3f", delISX);
  SetVariable("IMAGETICKTIME", report, VARTYPE_REGULAR + VARTYPE_ADD_FOR_NUM, -1, false);
  return 0;
}

// ImageMarkerPosition
int CMacCmd::ImageMarkerPosition(void)
{
  CString report;
  EMimageBuffer *imBuf;
  float delX = -1., delY = -1.;
  int index;

  if (mStrItems[1] == "0" || mStrItems[1] == "-1") {
    imBuf = mWinApp->mMainView->GetActiveImBuf();
    if (!imBuf || !imBuf->mImage)
      ABORT_LINE("There is no image in the active buffer for line\n\n:");
  } else {
    if (ConvertBufferLetter(mStrItems[1], 0, true, index, report, true))
      ABORT_LINE(report);
    imBuf = ImBufForIndex(index);
  }
  if (imBuf->mHasUserPt) {
    delX = imBuf->mUserPtX;
    delY = imBuf->mUserPtY;
    mLogRpt.Format("Marker point coordinates %.1f, %.1f", delX, delY);
  } else
    mLogRpt = "The image has no marker point set";
  SetRepValsAndVars(2, delX, delY);
  return 0;
}

// ImageConditions
int CMacCmd::ImageConditions(void)
{
  CString report;
  EMimageBuffer *imBuf;
  EMimageExtra *extra;
  int index;
  if (ConvertBufferLetter(mStrItems[1], 0, true, index, report))
    ABORT_LINE(report);
  imBuf = ImBufForIndex(index);
  extra = imBuf->mImage->GetUserData();
  if (!extra)
    ABORT_LINE("The image has no extra data structure for line:\n\n");
  mLogRpt.Format("Buffer %c: electron dose %.3f  magnification %d", 'A' + index, 
    extra->m_fDose, extra->m_iMag);
  SetRepValsAndVars(2, extra->m_fDose, extra->m_iMag);
  return 0;
}

// ImageLowDoseSet
int CMacCmd::ImageLowDoseSet(void)
{
  CString report;
  double delX;
  int index, index2, mag;

  if (ConvertBufferLetter(mStrItems[1], 0, true, index, report))
    ABORT_LINE(report);
  index2 = mImBufs[index].mConSetUsed;

  if (index2 == MONTAGE_CONSET)
    index2 = RECORD_CONSET;
  if (mImBufs[index].mLowDoseArea && index2 >= 0) {
    mLogRpt = "Image is from " + mModeNames[index2] + " in Low Dose";
  } else {
    if (index2 == TRACK_CONSET) {

      // Try to match View vs R by mag first
      mag = mImBufs[index].mMagInd;
      if (mLdParam[VIEW_CONSET].magIndex == mag) {
        index2 = VIEW_CONSET;
      } else if (mLdParam[RECORD_CONSET].magIndex == mag) {

        // Then decide between preview and record based on exposure
        delX = mImBufs[index].mExposure;
        if (fabs(delX - mConSets[RECORD_CONSET].exposure) <
          fabs(delX - mConSets[PREVIEW_CONSET].exposure))
          index2 = RECORD_CONSET;
        else
          index2 = PREVIEW_CONSET;
      }
      mLogRpt = "Image matches one from " + mModeNames[index2] + " in Low Dose";
    } else if (index2 >= 0) {
      mLogRpt = "Image is from " + mModeNames[index2] + " parameters, not in Low Dose";
    }
  }
  if (index2 > SEARCH_CONSET || index2 < 0) {
    index2 = -1;
    mLogRpt = "Image properties do not match any Low Dose area well enough";
  }
  SetRepValsAndVars(2, (double)index2,
    mImBufs[index].mLowDoseArea ? 1. : 0.);
  return 0;
}

// MeasureBeamSize, MeasureBeamPosition
int CMacCmd::MeasureBeamSize(void)
{
  CString report;
  int index, binning, ix0, numQuad;
  float xcen, ycen, xcenUse, ycenUse, radUse, fracUse, radius, shiftX, shiftY, fitErr;
  float pixel;

  if (ConvertBufferLetter(mStrItems[1], 0, true, index, report))
    ABORT_LINE(report);
  ix0 = mProcessImage->FindBeamCenter(&mImBufs[index], xcen, ycen,
    radius, xcenUse, ycenUse, radUse, fracUse, binning, numQuad, shiftX, shiftY, fitErr);
  if (ix0)
    ABORT_LINE("No beam edges were detected in image for line:\n\n");
  if (CMD_IS(MEASUREBEAMPOSITION)) {
    xcen = (float)(xcen * binning - mImBufs[index].mImage->getWidth() / 2.);
    ycen = (float)(ycen * binning - mImBufs[index].mImage->getHeight() / 2.);
    mLogRpt.Format("Beam offset from image center measured to be %.1f  %.1f pixels, from"
      " %d quadrants, fit error %.3f", xcen, ycen, numQuad, fitErr);
    SetReportedValues(xcen, ycen, numQuad, fitErr);
  } else {
    pixel = mShiftManager->GetPixelSize(&mImBufs[index]);
    if (!pixel)
      ABORT_LINE("No pixel size is available for the image for line:\n\n");
    radius *= 2.f * binning * pixel;
    mLogRpt.Format("Beam diameter measured to be %.3f um from %d quadrants, fit error"
      " %.3f", radius, numQuad, fitErr);
    SetReportedValues(radius, numQuad, fitErr);
  }
  return 0;
}

// MeasureBeamEllipse
int CMacCmd::MeasureBeamEllipse(void)
{
  CString report;
  int index;
  float xcen, ycen, longAxis, shortAxis, angle;
  if (ConvertBufferLetter(mStrItems[1], 0, true, index, report))
    ABORT_LINE(report);
  if (mProcessImage->FindEllipticalBeamParams(&mImBufs[index], xcen, ycen, longAxis,
    shortAxis, angle))
    ABORT_LINE("Image analysis failed for line:\n\n");
  mLogRpt.Format("Offset %.1f %.1f pixels, long and short axes %.1f and %.1f pixels, "
    "long axis at %.1f degrees", xcen, ycen, longAxis, shortAxis, angle);
  SetReportedValues(xcen, ycen, longAxis, shortAxis, angle);
  return 0;
}

// QuadrantMeans
int CMacCmd::QuadrantMeans(void)
{
  CString report;
  KImage *image;
  double delX, delY, h1, v1, v2, h2, h3, v3, v4, h4;
  int index, ix0, ix1, iy0, iy1, sizeX, sizeY;

  delY = 0.1;
  delX = 0.1;
  index = 2;
  if (!mItemEmpty[1])
    index = mItemInt[1];
  if (!mItemEmpty[2])
    delX = mItemDbl[2];
  if (!mItemEmpty[3])
    delY = mItemDbl[3];
  image = mImBufs->mImage;
  if (image)
    image->getSize(sizeX, sizeY);
  if (!image || index > B3DMIN(sizeX, sizeY) / 4 || index < 0 || delX <= 0.
    || delY < 0. || delX > 0.5 || delY > 0.4)
      ABORT_LINE("Parameter out of bounds for image, or no image in A in statement:"
      "\n\n");
  ix0 = B3DMAX((int)(delY * sizeX / 2), 1);   // Trim length in X
  iy0 = B3DMAX((int)(delY * sizeY / 2), 1);   // Trim length in Y
  ix1 = B3DMAX((int)(delX * sizeX / 2), 1);   // Width in X
  iy1 = B3DMAX((int)(delX * sizeY / 2), 1);   // Width in Y
  h4 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
    sizeX / 2 + index, sizeX - 1 - ix0, sizeY / 2 + index,
    sizeY / 2 + index + iy1 - 1);
  v4 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
    sizeX / 2 + index, sizeX / 2 + index + ix1 - 1, sizeY / 2 + index,
    sizeY - 1 - iy0);
  v3 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
    sizeX / 2 - index - ix1, sizeX / 2 - index - 1, sizeY / 2 + index,
    sizeY - 1 - iy0);
  h3 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
    ix0, sizeX / 2 - index - 1, sizeY / 2 + index, sizeY / 2 + index + iy1 - 1);
  h2 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
    ix0, sizeX / 2 - index - 1, sizeY / 2 - index - iy1, sizeY / 2 - index - 1);
  v2 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
    sizeX / 2 - index - ix1, sizeX / 2 - index - 1, iy0, sizeY / 2 - index - 1);
  v1 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
    sizeX / 2 + index, sizeX / 2 + index + ix1 - 1, iy0, sizeY / 2 - index - 1);
  h1 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
    sizeX / 2 + index, sizeX - 1 - ix0, sizeY / 2 - index - iy1,
    sizeY / 2 - index - 1);
  report.Format("h1, v1, v2, h2, h3, v3, v4, h4:  %.2f   %.2f   %.2f   %.2f   %.2f   "
    "%.2f   %.2f   %.2f", h1, v1, v2, h2, h3, v3, v4, h4);
  mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
  return 0;
}

// CheckForBadStripe
int CMacCmd::CheckForBadStripe(void)
{
  CString report;
  int index, index2, ix0, ix1;

  if (ConvertBufferLetter(mStrItems[1], 0, true, index, report))
    ABORT_LINE(report);
  ix0 = mItemInt[2];
  if (mItemEmpty[2]) {
    if (mImBufs[index].mCamera < 0)
      ABORT_LINE("The image has no camera defined and requires an entry for horizontal"
        " versus vertical analysis in:\n\n");
    ix0 = (mWinApp->GetCamParams() + mImBufs[index].mCamera)->rotationFlip % 2;
  }
  index2 = mProcessImage->CheckForBadStripe(&mImBufs[index], ix0, ix1);
  if (!index2)
    mLogRpt = "No bad stripes detected";
  else if (ix1 < 0)
    mLogRpt.Format("Bad stripes detected with %d sharp transitions; 1 near first or last "
      "expected boundary", index2);
  else
    mLogRpt.Format("Bad stripes detected with %d sharp transitions; %d near expected"
      " boundaries", index2, ix1);
  SetReportedValues(index2, ix1);
  return 0;
}

// CircleFromPoints
int CMacCmd::CircleFromPoints(void)
{
  CString report;
  double h1, v1, v2, h2, h3, v3;
  float radius, xcen, ycen;
  if (mItemEmpty[6])
    ABORT_LINE("Need three sets of X, Y coordinates on line:\n" + mStrLine);
  h1 = mItemDbl[1];
  v1 = mItemDbl[2];
  h2 = mItemDbl[3];
  v2 = mItemDbl[4];
  h3 = mItemDbl[5];
  v3 = mItemDbl[6];
  if (circleThrough3Pts((float)h1, (float)v1, (float)h2, (float)v2, (float)h3,
    (float)v3, &radius, &xcen, &ycen)) {
      mWinApp->AppendToLog("The three points are too close to being on a straight line"
        , LOG_OPEN_IF_CLOSED);
  } else {
    report.Format("Circle center = %.2f  %.2f  radius = %.2f", xcen, ycen, radius);
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    SetRepValsAndVars(7, xcen, ycen, radius);
  }
  return 0;
}

// FindPixelSize
int CMacCmd::FindPixelSize(void)
{
  float dist = 0., vectors[4];
  mProcessImage->FindPixelSize(0., 0., 0., 0., 0, 0, dist, vectors);
  return 0;
}

// AutoCorrPeakVectors
int CMacCmd::AutoCorrPeakVectors(void)
{
  float vectors[4], spacing = mItemFlt[2];
  int bufInd, flags = FIND_ACPK_NO_WAFFLE;
  CString report;
  if (ConvertBufferLetter(mStrItems[1], -1, true, bufInd, report))
    ABORT_LINE(report);
  if (!mItemEmpty[3] && mItemInt[3])
    flags |= FIND_PIX_NO_TRIM;
  if (!mItemEmpty[4] && mItemInt[4])
    flags |= FIND_PIX_NO_DISPLAY;
  if (mProcessImage->FindPixelSize(0., 0., 0., 0., bufInd, flags, spacing, vectors))
    return 1;
  mLogRpt.Format("Average spacing is %.1f pixels, vectors %.1f, %.1f and %.1f, %.1f",
    spacing, vectors[0], vectors[1], vectors[2], vectors[3]);
  SetReportedValues(spacing, vectors[0], vectors[1], vectors[2], vectors[3]);
  return 0;
}

// Echo, EchoNoVarSub, EchoBreakLines
int CMacCmd::Echo(void)
{
  CString report;
  bool breakl = CMD_IS(ECHOBREAKLINES);

  if ((CMD_IS(ECHO) || breakl) && !mRunningScrpLang)
    SubstituteVariables(&mStrLine, 1, mStrLine);
  JustStripItems(mStrLine, 1, report, true);
  if (breakl)
    report.Replace("\\n", "\r\n");

  // Preserve line ending, convert array separators to spaces
  if (report.Find("\n") >= 0) {
    report.Replace("\r\n", "\r|#n");
    report.Replace("\n", breakl ? "\r\n" : "  ");
    report.Replace("\r|#n", "\r\n");
  }
  mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
  return 0;
}

// EchoEval, EchoReplaceLine, EchoNoLineEnd
int CMacCmd::EchoEval(void)
{
  CString report;
  int index, index2;

  report = "";
  for (index = 1; index < MAX_MACRO_TOKENS && !mItemEmpty[index]; index++) {
    if (index > 1)
      report += " ";
    report += mStrItems[index];
  }
  report.Replace("\n", "  ");
  index2 = 0;
  if (CMD_IS(ECHOREPLACELINE))
    index2 = 3;
  else if (CMD_IS(ECHONOLINEEND))
    index2 = 1;
  mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED, index2);
  return 0;
}

// verbose
int CMacCmd::Verbose(void)
{
  mVerbose = mItemInt[1];
  return 0;
}

// ProgramTimeStamps
int CMacCmd::ProgramTimeStamps(void)
{
  const char **months = mWinApp->mDocWnd->GetMonthStrings();
  if (mItemInt[1] < 0) {
    CTime ctdt = CTime::GetCurrentTime();
    PrintfToLog("Current date  %s %d %d  %02d:%02d:%02d", months[ctdt.GetMonth() - 1],
      ctdt.GetDay(), ctdt.GetYear(), ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond());
    return 0;
  }
  mWinApp->AppendToLog(mWinApp->GetStartupMessage(mItemInt[1] != 0));
  if (mItemInt[2] != 0)
    mWinApp->mParamIO->ReportSpecialOptions();
  return 0;
}

// IsVersionAtLeast, SkipIfVersionLessThan
int CMacCmd::IsVersionAtLeast(void)
{
  BOOL truth;
  int index2, ix0, ix1;

  index2 = mItemEmpty[2] ? 0 : mItemInt[2];
  ix0 = mWinApp->GetIntegerVersion();
  ix1 = mWinApp->GetBuildDayStamp();
  truth = mItemInt[1] <= ix0 && index2 <= ix1;
  if (CMD_IS(ISVERSIONATLEAST)) {
    SetReportedValues(truth ? 1. : 0.);
    mLogRpt.Format("Program version is %d date %d, %s than %d %s", ix0, ix1,
      truth ? "later" : "earlier", mItemInt[1], (LPCTSTR)mStrItems[2]);
  } else if (!truth && mCurrentIndex < mMacro->GetLength())
    GetNextLine(mMacro, mCurrentIndex, mStrLine);
  return 0;
}

int CMacCmd::IsFFTWindowOpen(void)
{
  int index;

  index = mWinApp->mFFTView ? 1 : 0;
  mLogRpt.Format("FFT window %s open", index ? "IS" : "is NOT");
  SetRepValsAndVars(1, index);
  return 0;
}

// ReportEnvironVar
int CMacCmd::ReportEnvironVar(void)
{
  char *envar;
  SubstituteVariables(&mStrItems[1], 1, mStrLine);
  envar = getenv((LPCTSTR)mStrItems[1]);
  if (envar)
    mLogRpt.Format("Environment variable %s = %s", (LPCTSTR)mStrItems[1], envar);
  else
    mLogRpt.Format("Environment variable %s is not set", (LPCTSTR)mStrItems[1]);
  SetOneReportedValue(&mStrItems[2], envar ? 1. : 0., 1);
  SetOneReportedValue(&mStrItems[2], CString(envar ? envar :"none"), 2);
  return 0;
}

// ReportSettingsFile
int CMacCmd::ReportSettingsFile(void)
{
  CString report;

  report = mWinApp->mDocWnd->GetCurrentSettingsPath();
  mWinApp->AppendToLog("Current settings file: " + report, mLogAction);
  SetOneReportedValue(&mStrItems[1], report, 1);
  report = "None";
  if (mWinApp->mDocWnd->GetReadScriptPack()) {
    report = mWinApp->mDocWnd->GetCurScriptPackPath();
    mWinApp->AppendToLog("Current script package: " + report, mLogAction);
  }
  SetOneReportedValue(&mStrItems[1], report, 2);
  return 0;
}

// ReportSystemPath
int CMacCmd::ReportSystemPath(void)
{
  mStrCopy = mWinApp->mDocWnd->GetSystemPath();
  mLogRpt = "System path is: " + mStrCopy;
  SetOneReportedValue(&mStrItems[1], mStrCopy, 1);
  return 0;
}

// ReportRotationProblems
int CMacCmd::ReportRotationProblems(void)
{
  if (!mShiftManager->ReportFallbackRotations(false))
    mLogRpt = "There is good calibrated rotation information for magnifications "
    "with other calibrations";
  return 0;
}

// CheckStageToCamera
int CMacCmd::CheckStageToCamera(void)
{
  int ret = mShiftManager->CheckStageToCamConsistency(mItemEmpty[1] ? 8.f : mItemFlt[1],
    mItemEmpty[2] ? 0.08f : mItemFlt[2], !mItemEmpty[3] && mItemInt[3] != 0);
  SetRepValsAndVars(3, ret);
  return 0;
}

// ListAllCalibrations
int CMacCmd::ListAllCalibrations(void)
{
  bool saveDefer = mDeferLogUpdates;
  mDeferLogUpdates = true;
  mWinApp->mMenuTargets.DoListISVectors(true);
  mWinApp->mMenuTargets.DoListISVectors(false);
  mWinApp->mMenuTargets.DoListStageCals();
  mFocusManager->OnAutofocusListCalibrations();
  mShiftManager->ListBeamShiftCals();
  mWinApp->mBeamAssessor->ListIntensityCalibrations();
  mWinApp->mBeamAssessor->ListSpotCalibrations();
  mShiftManager->ReportFallbackRotations(false);
  mShiftManager->CheckStageToCamConsistency(8.f, 0.08f, true);
  mWinApp->mMenuTargets.OnListFocusMagCals();
  mDeferLogUpdates = saveDefer;
  if (!mDeferLogUpdates && mWinApp->mLogWindow)
    mWinApp->mLogWindow->FlushDeferredLines();
  return 0;
}

// ListISVectorsToGraph
int CMacCmd::ListISVectorsToGraph(void)
{
  bool saveDefer = mDeferLogUpdates;
  mDeferLogUpdates = true;
  mWinApp->mMenuTargets.SaveNextISVectorsForGraph(mItemInt[2] - 1, mItemInt[3]);
  mWinApp->mMenuTargets.DoListISVectors(mItemInt[1] != 0);
  mDeferLogUpdates = saveDefer;
  if (!mDeferLogUpdates && mWinApp->mLogWindow)
    mWinApp->mLogWindow->FlushDeferredLines();
  return 0;
}

// Pause, YesNoBox, PauseIfFailed, AbortIfFailed
int CMacCmd::Pause(void)
{
  CString report;
  BOOL doPause, doAbort;
  int index;

  doPause = CMD_IS(PAUSEIFFAILED);
  doAbort = CMD_IS(ABORTIFFAILED);
  if (!(doPause || doAbort) || !mLastTestResult) {
    SubstituteLineStripItems(mStrLine, 1, report);
    if (doPause || doAbort)
      mWinApp->AppendToLog(report);
    doPause = doPause || CMD_IS(PAUSE);
    if (doPause) {
      report += "\n\nDo you want to proceed with the script?";
      index = AfxMessageBox(report, doAbort ? MB_EXCLAME : MB_QUESTION);
    } else {
      if (doAbort || !mNoLineWrapInMessageBox)
        index = SEMMessageBox(report, doAbort ? MB_EXCLAME : MB_QUESTION);
      else
        index = SEMThreeChoiceBox(report, "Yes", "No", "", MB_QUESTION, 0, 0, 0, true);
    }
    if ((doPause && index == IDNO) || doAbort) {
      SuspendMacro(doAbort);
      return 1;
    } else
      SetReportedValues(index == IDYES ? 1. : 0.);
  }
  return 0;
}

// OKBox
int CMacCmd::OKBox(void)
{
  CString report;

  SubstituteLineStripItems(mStrLine, 1, report);
  if (mNoLineWrapInMessageBox)
    SEMThreeChoiceBox(report, "OK", "", "", MB_OK | MB_ICONINFORMATION, 0, 0, 0, true);
  else
    AfxMessageBox(report, MB_OK | MB_ICONINFORMATION);
  return 0;
}

// EnterOneNumber, EnterDefaultedNumber
int CMacCmd::EnterOneNumber(void)
{
  BOOL truth;
  int index, index2, ix0;
  float backlashX;

  backlashX = 0.;
  index = 1;
  index2 = 3;
  if (CMD_IS(ENTERDEFAULTEDNUMBER)) {

    // Here, enter the value and the number of digits, or < 0 to get an integer entry
    backlashX = mItemFlt[1];
    index = 3;
    index2 = mItemInt[2];
    if (index2 < 0)
      ix0 = mItemInt[1];
  }
  SubstituteLineStripItems(mStrLine, index, mStrCopy);
  if (index2 >= 0) {
    truth = KGetOneFloat(mStrCopy, backlashX, index2);
  } else {
    truth = KGetOneInt(mStrCopy, ix0);
    backlashX = (float)ix0;
  }
  if (!truth)
    SUSPEND_NOLINE("because no number was entered");
  mLogRpt.Format("%s: user entered  %g", (LPCTSTR)mStrCopy, backlashX);
  SetReportedValues(backlashX);
  return 0;
}

// EnterString
int CMacCmd::EnterString(void)
{
  CString report;

  mStrCopy = "Enter a text string:";
  report = "";
  SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  if (!KGetOneString(mStrCopy, report))
    SUSPEND_NOLINE("because no string was entered");
  if (SetVariable(mItem1upper, report, VARTYPE_REGULAR, -1, false))
    ABORT_NOLINE("Error setting variable " + mStrItems[1] + " with string " + report);
  return 0;
}

// ThreeChoiceBox
int CMacCmd::ThreeChoiceBox(void)
{
  CString report;
  int index, index2, i, ix0, ix1, numButtons = 3;
  CString *valPtr;
  Variable *vars[5];
  CString buttons[3];
  if (mItemEmpty[5])
    numButtons = 2;
  if (mItemEmpty[4])
    numButtons = 1;
  for (i = 0; i < numButtons + 2; i++) {
    report = mStrItems[i + 1];
    report.MakeUpper();
    vars[i] = LookupVariable(report, index2);
    if (!vars[i])
      ABORT_LINE("The variable " + mStrItems[i + 1] + " is not defined in line:\n\n");
    if (vars[i]->rowsFor2d)
      ABORT_LINE("The variable " + mStrItems[i + 1] + " is a 2D array which is not "
        "allowed for line:\n\n");
  }
  report = "";
  for (i = 0; i < numButtons + 1; i++) {
    if (!vars[i]->value.IsEmpty()) {
      if (!report.IsEmpty())
        report += "\n\n";
      report += vars[i]->value;
    }
  }

  valPtr = &vars[numButtons + 1]->value;
  for (i = 0; i < numButtons; i++) {
    FindValueAtIndex(*valPtr, i + 1, ix0, ix1);
    buttons[i] = valPtr->Mid(ix0, ix1 - ix0);
  }
  i = SEMThreeChoiceBox(report, buttons[0], buttons[1], buttons[2], 
    B3DCHOICE(numButtons == 1, MB_OK | MB_ICONINFORMATION, 
    (numButtons == 2 ? MB_YESNO : MB_YESNOCANCEL) | MB_ICONQUESTION), 0, false, 0, 
    mNoLineWrapInMessageBox);
  if (i == IDYES)
    index = 1;
  else if (i == IDNO)
    index = 2;
  else
    index = 3;
  SetReportedValues(index);
  if (numButtons > 1)
    mLogRpt = "\"" + buttons[index - 1] + "\" was chosen";
  return 0;
}

// NoLineWrapInMessageBox
int CMacCmd::NoLineWrapInMessageBox(void)
{
  mNoLineWrapInMessageBox = mItemEmpty[1] || mItemInt[1];
  return 0;
}

// SetStatusLine
int CMacCmd::SetStatusLine(void)
{
  if (mItemInt[1] < 1 || mItemInt[1] > NUM_CM_MESSAGE_LINES)
    ABORT_LINE("Status line number out of range in line:\n\n");
  SubstituteLineStripItems(mStrLine, 2, mStatusLines[mItemInt[1] - 1]);
  if (mItemInt[1] > mNumStatusLines)
    SetNumStatusLines(mItemInt[1]);
  else
    mWinApp->mCameraMacroTools.Invalidate();
  return 0;
}

// ClearStatusLine, HighlightStatusLine
int CMacCmd::ClearStatusLine(void)
{
  int ind, start = mItemInt[1], end = mItemInt[1];
  if (start < 0 || start > NUM_CM_MESSAGE_LINES)
    ABORT_LINE("Status line number out of range in line:\n\n");
  if (!start) {
    start = 1;
    end = NUM_CM_MESSAGE_LINES;
  }
  for (ind = start; ind <= end; ind++) {
    if (CMD_IS(CLEARSTATUSLINE)) {
      mStatusLines[ind - 1] = "";
      mHighlightStatus[ind - 1] = 0;
    } else
      mHighlightStatus[ind - 1] = mItemInt[2] != 0;
  }
  mWinApp->mCameraMacroTools.Invalidate();
  return 0;
}

// CompareNoCase, CompareStrings
int CMacCmd::CompareNoCase(void)
{
  int index, index2;
  Variable *var;

  var = LookupVariable(mItem1upper, index2);
  if (!var)
    ABORT_LINE("The variable " + mStrItems[1] + " is not defined in line:\n\n");
  SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  if (CMD_IS(COMPARESTRINGS))
    index = var->value.Compare(mStrCopy);
  else
    index = var->value.CompareNoCase(mStrCopy);
  mLogRpt.Format("The strings %s equal", index ? "are NOT" : "ARE");
  SetReportedValues(index);
  return 0;
}

// TrimString
int CMacCmd::TrimString(void)
{
  int index, index2;
  Variable *var;
  bool front = mItemInt[2] != 0;

  var = LookupVariable(mItem1upper, index2);
  if (!var)
    ABORT_LINE("The variable " + mStrItems[1] + " is not defined in line:\n\n");

  index = var->value.Find(mStrItems[3]);
  if (index < 0)
    ABORT_LINE("The string \"" + var->value + "\" does not contain \"" + mStrItems[3] +
      "\" in line:\n\n");
  if (front) {
    mStrCopy = var->value.Mid(index + mStrItems[3].GetLength());
  } else {
    for (;;) {
      index2 = var->value.Find(mStrItems[3], index + 1);
      if (index2 < 0)
        break;
      index = index2;
    }
    mStrCopy = var->value.Left(index);
  }
  SetOneReportedValue(&mStrItems[4], mStrCopy, 1);
  return 0;
}

// StripEndingDigits
int CMacCmd::StripEndingDigits(void)
{
  CString report;
  int index, index2;
  Variable *var;

  var = LookupVariable(mItem1upper, index2);
  if (!var)
    ABORT_LINE("The variable " + mStrItems[1] + " is not defined in line:\n\n");
  report = var->value;
  for (index = report.GetLength() - 1; index > 0; index--)
    if ((int)report.GetAt(index) < '0' || (int)report.GetAt(index) > '9')
      break;
  mStrCopy = report.Right(report.GetLength() - (index + 1));
  report = report.Left(index + 1);
  mItem1upper = mStrItems[2];
  mItem1upper.MakeUpper();
  if (SetVariable(mItem1upper, report, VARTYPE_REGULAR + VARTYPE_ADD_FOR_NUM, -1, false))
    ABORT_LINE("Error setting variable " + mStrItems[2] + " with string " + report +
    " in:\n\n");
  SetReportedValues(atoi((LPCTSTR)mStrCopy));
  return 0;
}

// SetNextEmailAddress
int CMacCmd::SetNextEmailAddress(void)
{
  if (mItemInt < 0)
    mStrCopy = "";
  else
    SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  mWinApp->mMailer->SetNextEmailAddress(mStrCopy, mItemInt[1] > 0);
  return 0;
}

// MailSubject
int CMacCmd::MailSubject(void)
{
  SubstituteLineStripItems(mStrLine, 1, mMailSubject);
  return 0;
}

// SendEmail, ErrorBoxSendEmail
int CMacCmd::SendEmail(void)
{
  CString report;

  SubstituteLineStripItems(mStrLine, 1, report);
  if (CMD_IS(SENDEMAIL)) {
    mWinApp->mMailer->SendMail(mMailSubject, report);
  } else {
    if (!mWinApp->mMailer->GetInitialized() || mWinApp->mMailer->GetSendTo().IsEmpty())
      ABORT_LINE("Mail server not initialized or email address not defined; cannot "
      "do:\n");
    mEmailOnError = report;
  }
  return 0;
}

// ClearAlignment
int CMacCmd::ClearAlignment(void)
{
  BOOL doShift;

  doShift = (mItemEmpty[1] || !mItemInt[1]) && !mScope->GetNoScope();
  mShiftManager->SetAlignShifts(0., 0., false, mImBufs, doShift);
  return 0;
}

// ResetImageShift
int CMacCmd::ResetImageShift(void)
{
  BOOL truth;
  int index;
  float backlashX;

  truth = mShiftManager->GetBacklashMouseAndISR();
  backlashX = 0.;
  if (mItemInt[1] > 0) {
    mShiftManager->SetBacklashMouseAndISR(true);
    if (mItemInt[1] > 1)
      backlashX = mItemEmpty[2] ? mScope->GetStageRelaxation() : mItemFlt[2];
  }
  index = mShiftManager->ResetImageShift(true, false, 10000, backlashX);
  mShiftManager->SetBacklashMouseAndISR(truth);
  if (index) {
    mCurrentIndex = mLastIndex;
    SuspendMacro();
    return 1;
  }
  return 0;
}

// ResetShiftIfAbove
int CMacCmd::ResetShiftIfAbove(void)
{
  ScaleMat aMat;
  double delISX, delISY, delX, delY, specDist;

  if (mItemEmpty[1])
    ABORT_LINE("ResetShiftIfAbove must be followed by a number in: \n\n");
  mScope->GetLDCenteredShift(delISX, delISY);
  aMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
  delX = aMat.xpx * delISX + aMat.xpy * delISY;
  delY = aMat.ypx * delISX + aMat.ypy * delISY;
  specDist = sqrt(delX * delX + delY * delY);
  if (specDist > mItemDbl[1])
    mWinApp->mComplexTasks->ResetShiftRealign();
  return 0;
}

// Eucentricity
int CMacCmd::Eucentricity(void)
{
  CString report;
  int index;

  index = FIND_EUCENTRICITY_FINE;
  if (!mItemEmpty[1])
    index = mItemInt[1];
  if (index < 0) {
    if (mWinApp->mParticleTasks->EucentricityFromFocus(mItemEmpty[2] ?-1 : mItemInt[2])) {
      AbortMacro();
      return 1;
    }
    return 0;
  }
  if ((index & (FIND_EUCENTRICITY_FINE | FIND_EUCENTRICITY_COARSE)) == 0) {
    report.Format("Error in script: value on Eucentricity statement \r\n"
      "should be %d for coarse, %d for fine, or %d for both",
      FIND_EUCENTRICITY_COARSE, FIND_EUCENTRICITY_FINE,
      FIND_EUCENTRICITY_FINE | FIND_EUCENTRICITY_COARSE);
    ABORT_LINE(report);
  }
  mWinApp->mComplexTasks->FindEucentricity(index);
  return 0;
}

// ReportLastAxisOffset
int CMacCmd::ReportLastAxisOffset(void)
{
  double delX;

  delX = mWinApp->mComplexTasks->GetLastAxisOffset();
  if (delX < -900)
    ABORT_NOLINE("There is no last axis offset; fine eucentricity has not been run");
  mLogRpt.Format("Lateral axis offset in last run of Fine Eucentricity was %.2f", delX);
  SetRepValsAndVars(1, delX);
  return 0;
}

// SetTiltAxisOffset
int CMacCmd::SetTiltAxisOffset(void)
{
  if (fabs(mItemDbl[1]) > 25.)
    ABORT_LINE("The tilt axis offset must be less than 25 microns in line:\n\n");
  mScope->SetTiltAxisOffset(mItemFlt[1]);
  return 0;
}

// LimitNextRoughEucen
int CMacCmd::LimitNextRoughEucen(void)
{
  if (mItemFlt[1] < 0)
    ABORT_LINE("Eucentricity change limit cannot be negative in:\n\n");
  mWinApp->mComplexTasks->SetFENextCoarseMaxDeltaZ(mItemFlt[1]);
  return 0;
}

// ReportRoughEucenFailed
int CMacCmd::ReportRoughEucenFailed(void)
{
  int failed = mWinApp->mComplexTasks->GetFELastCoarseFailed() ? 1 : 0;
  mLogRpt.Format("Last rough eucentricity %s", failed ? "failed" : "succeeded");
  SetRepValsAndVars(1, failed);
  return 0;
}

// WalkUpTo
int CMacCmd::WalkUpTo(void)
{
  double delISX;

  if (mItemEmpty[1])
    ABORT_LINE("WalkUpTo must be followed by a target angle in: \n\n");
  delISX = mItemDbl[1];
  if (delISX < -80. || delISX > 80.)
    ABORT_LINE("Target angle is too high in: \n\n");
  mWinApp->mComplexTasks->WalkUp((float)delISX, -1, 0);
  return 0;
}

// ReverseTilt
int CMacCmd::ReverseTilt(void)
{
  int index;

  index = (mScope->GetReversalTilt() > mScope->GetTiltAngle()) ? 1 : -1;
  if (!mItemEmpty[1]) {
    index = mItemInt[1];
    if (index > 0)
      index = 1;
    else if (index < 0)
      index = -1;
    else
      ABORT_NOLINE("Error in script: ReverseTilt should not be followed by 0");
  }
  mWinApp->mComplexTasks->ReverseTilt(index);
  return 0;
}

// DriftWaitTask
int CMacCmd::DriftWaitTask(void)
{
  DriftWaitParams *dfltParam = mWinApp->mParticleTasks->GetDriftWaitParams();
  DriftWaitParams dwparm = *dfltParam;
  if (!mItemEmpty[2]) {
    if (mStrItems[2] == "nm")
      dwparm.useAngstroms = false;
    else if (mStrItems[2] == "A")
      dwparm.useAngstroms = true;
    else if (mStrItems[2] != "0")
      ABORT_LINE("The second entry should be \"nm\", \"A\", or \"0\" in line:\n\n");
  }
  if (!mItemEmpty[1] && mItemDbl[1] > 0.) {
    dwparm.driftRate = mItemFlt[1] / (dwparm.useAngstroms ? 10.f : 1.f);
  }
  if (!mItemEmpty[3] && mItemDbl[3] > 0.) {
    dwparm.maxWaitTime = mItemFlt[3];
  }
  if (!mItemEmpty[4] && mItemDbl[4] > 0.) {
    dwparm.interval = mItemFlt[4];
  }
  if (mItemInt[5])
    dwparm.failureAction = mItemInt[5] > 0 ? 1 : 0;
  if (!mItemEmpty[6]) {
    if (mStrItems[6] == "T")
      dwparm.measureType = WFD_USE_TRIAL;
    else if (mStrItems[6] == "F")
      dwparm.measureType = WFD_USE_FOCUS;
    else if (mStrItems[6] == "A")
      dwparm.measureType = WFD_WITHIN_AUTOFOC;
    else if (mStrItems[6] != "0")
      ABORT_LINE("The image type to measure defocus from must be one of T, F, "
        "A, or 0 in line:\n\n");
  }
  if (mItemInt[7])
    dwparm.changeIS = mItemInt[7] > 0 ? 1 : 0;
  if (!mItemEmpty[8]) {
    dwparm.setTrialParams = true;
    if (mItemDbl[8] > 0)
      dwparm.exposure = mItemFlt[8];
    if (!mItemEmpty[9])
      dwparm.binning = mItemInt[9];
  }
  mWinApp->mParticleTasks->WaitForDrift(dwparm, false);
  return 0;
}

// ConditionPhasePlate
int CMacCmd::ConditionPhasePlate(void)
{
  if (mWinApp->mMultiTSTasks->ConditionPhasePlate(mItemInt[1] != 0)) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// GetWaitTaskDrift
int CMacCmd::GetWaitTaskDrift(void)
{
  SetRepValsAndVars(1, mWinApp->mParticleTasks->GetWDLastDriftRate(),
    mWinApp->mParticleTasks->GetWDLastFailed());
  return 0;
}

// BacklashAdjust
int CMacCmd::BacklashAdjust(void)
{
  float backlashX, backlashY;

  mWinApp->mMontageController->GetColumnBacklash(backlashX, backlashY);
  mWinApp->mComplexTasks->BacklashAdjustStagePos(backlashX, backlashY, false, false);
  return 0;
}

// CenterBeamFromImage
int CMacCmd::CenterBeamFromImage(void)
{
  BOOL truth;
  double delISX;
  int index;

  truth = mItemInt[1] != 0;
  delISX = !mItemEmpty[2] ? mItemDbl[2] : 0.;
  index = mProcessImage->CenterBeamFromActiveImage(0., 0., truth, delISX);
  if (index > 0 && index <= 3)
    ABORT_LINE("Script aborted centering beam because of no image,\n"
    "unusable image type, or failure to get memory");
  SetReportedValues(index);
  return 0;
}

// AutoCenterBeam
int CMacCmd::AutoCenterBeam(void)
{
  float maxShift = mItemEmpty[1] ? 0.f : mItemFlt[1];
  int pctSmaller = mItemEmpty[2] ? -1 : mItemInt[2];
  if (mWinApp->mAutocenDlg)
    ABORT_NOLINE("You cannot run beam autocentering with the setup dialog open");
  if (pctSmaller >= 0 && !mWinApp->LowDoseMode())
    ABORT_LINE("Low Dose mode must be on to autocenter with View in:\n\n");
  if (mWinApp->mMultiTSTasks->AutocenterBeam(maxShift, pctSmaller)) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// CookSpecimen
int CMacCmd::CookSpecimen(void)
{
  if (mWinApp->mMultiTSTasks->StartCooker()) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// SetIntensityByLastTilt, SetIntensityForMean, ChangeIntensityBy
int CMacCmd::SetIntensityByLastTilt(void)
{
  double delISX;
  int index, index2;

  index2 = mWinApp->LowDoseMode() ? 3 : -1;
  if (CMD_IS(SETINTENSITYFORMEAN)) {
    if (!mImBufs->mImage || mImBufs->IsProcessed() ||
      (mWinApp->LowDoseMode() &&
      mCamera->ConSetToLDArea(mImBufs->mConSetUsed) != 3))
      ABORT_LINE("Script aborted setting intensity because there\n"
        " is no image or a processed image in Buffer A\n"
        "(or, if in Low Dose mode, because the image in\n"
        " Buffer A is not from the Record area)");
    delISX = mItemDbl[1] /
      mProcessImage->EquivalentRecordMean(0);
  } else if (CMD_IS(SETINTENSITYBYLASTTILT)) {
    delISX = mIntensityFactor;
  } else {
    delISX = mItemDbl[1];
    if (delISX <= 0.)
      ABORT_LINE("The factor to change by must be positive in line:/n/n");
    index2 = mScope->GetLowDoseArea();
  }

  index = mWinApp->mBeamAssessor->ChangeBeamStrength(delISX, index2);
  if (CheckIntensityChangeReturn(index))
    return 1;
  UpdateLDAreaIfSaved();
  return 0;
}

// SetDoseRate
int CMacCmd::SetDoseRate(void)
{
  int index;

  if (mItemDbl[1] <= 0)
    ABORT_LINE("Dose rate must be positive for line:\n\n");
  if (!mImBufs->mImage)
    ABORT_LINE("There must be an image in buffer A for line:\n\n");
  index = mProcessImage->DoSetIntensity(true, mItemFlt[1]);
  if (index < 0) {
    AbortMacro();
    return 1;
  }
  if (CheckIntensityChangeReturn(index))
    return 1;
  UpdateLDAreaIfSaved();
  return 0;
}

// SetPercentC2, IncPercentC2
int CMacCmd::SetPercentC2(void)
{
  double delISX, delISY;
  int index;

  // The entered number is always in terms of % C2 or illuminated area, so for
  // incremental, first comparable value and add to get the absolute to set
  delISX = mItemDbl[1];
  if (mItemEmpty[1])
    ABORT_LINE("Script aborted because no C2 percent was entered in:\n\n");
  index = mScope->FastSpotSize();
  if (CMD_IS(INCPERCENTC2))
    delISX += mScope->GetC2Percent(index, mScope->GetIntensity());

  // Then convert to an intensity as appropriate for scope
  if (mScope->GetUseIllumAreaForC2()) {
    delISY = mScope->IllumAreaToIntensity(delISX / 100.);
  } else {
    delISY = (0.01 * delISX - mScope->GetC2SpotOffset(index)) /
      mScope->GetC2IntensityFactor();
  }
  mScope->SetIntensity(delISY);
  delISY = mScope->FastIntensity();
  delISX = mScope->GetC2Percent(index, delISY);
  mLogRpt.Format("Intensity set to %.3f%s  -  %.5f", delISX, mScope->GetC2Units(),
    delISY);
  UpdateLDAreaIfSaved();
  return 0;
}

// SetIlluminatedArea
int CMacCmd::SetIlluminatedArea(void)
{
  if (!mScope->SetIlluminatedArea(mItemDbl[1])) {
    AbortMacro();
    return 1;
  }
  UpdateLDAreaIfSaved();
  return 0;
}

// SetImageDistanceOffset
int CMacCmd::SetImageDistanceOffset(void)
{
  if (!mScope->SetImageDistanceOffset(mItemDbl[1])) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// SetAlpha
int CMacCmd::SetAlpha(void)
{
  if (!mScope->SetAlpha(mItemInt[1] - 1)) {
    AbortMacro();
    return 1;
  }
  UpdateLDAreaIfSaved();
  return 0;
}

// ReportJeolGIF
int CMacCmd::ReportJeolGIF(void)
{
  int index;

  index = mScope->GetJeolGIF();
  mLogRpt.Format("JEOL GIF MODE return value %d", index);
  SetRepValsAndVars(1, (double)index);
  return 0;
}

// SetJeolGIF
int CMacCmd::SetJeolGIF(void)
{
  if (!mScope->SetJeolGIF(mItemInt[1] ? 1 : 0)) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// NormalizeLenses
int CMacCmd::NormalizeLenses(void)
{
  int index;

  if (mItemInt[1] & 1)
    index = mScope->NormalizeProjector();
  if (mItemInt[1] & 2)
    index = mScope->NormalizeObjective();
  if (mItemInt[1] & 4)
    index = mScope->NormalizeCondenser();
  if (!index)
    AbortMacro();
  return 0;
}

// NormalizeAllLenses
int CMacCmd::NormalizeAllLenses(void)
{
  int index;

  index = 0;
  if (!mItemEmpty[1])
    index = mItemInt[1];
  if (index < 0 || index > 3)
    ABORT_LINE("Lens group specifier must be between 0 and 3 in: \n\n");
  if (!mScope->NormalizeAll(index))
    AbortMacro();
  return 0;
}

// ReportSlotStatus
int CMacCmd::ReportSlotStatus(void)
{
  int index, slot, station, id, angle, cartType;
  BOOL success;
  CString name, report, rowVal;
  if (mScope->GetJeolHasNitrogenClass() > 1) {
    if (!mScope->GetJeolLoaderInfo()->GetSize())
      ABORT_LINE("You need to do a cassette inventory; There is no cartridge "
        "information for:\n\n");
    FormatCartridgeInfo(mItemInt[1] -1, id, station, slot, cartType, angle, name, report,
      rowVal);
    if (id < 0) {
      mLogRpt.Format("There is no cassette information at index %d", mItemInt[1]);
      SetRepValsAndVars(2, id);
    } else {
      SetRepValsAndVars(2, id, station, slot, cartType, angle);
      SetOneReportedValue(&mStrItems[2], name, 6);
      mLogRpt.Format("Cartridge at index %d: %s", mItemInt[1], (LPCTSTR)report);
    }
    return 0;
  }

  success = mScope->CassetteSlotStatus(mItemInt[1], index, name);
  if (!success && name.IsEmpty()) {
    AbortMacro();
    return 1;
  }
  if (index < -1) {
    mLogRpt.Format("Requesting status of slot %d gives an error", mItemInt[1]);
  } else if (!mItemInt[1] && !name.IsEmpty()) {
    if (SetVariable("AUTOLOADERINFO", name, VARTYPE_PERSIST, -1, false, &report))
      ABORT_LINE(report);
    mLogRpt = "Array variable autoloaderInfo set with names for each slot";
    return 0;
  } else {
    mLogRpt.Format("Slot %d %s", mItemInt[1], index < 0 ? "has unknown status" :
      (index ? "is occupied" : "is empty"));
    if (!name.IsEmpty()) {
      if (success)
        mLogRpt += "; name = " + name;
      else
        mLogRpt = name + mLogRpt;
    }
  }
  SetRepValsAndVars(2, (double)index);
  if (!name.IsEmpty())
    SetOneReportedValue(&mStrItems[2], name, 2);
  return 0;
}

// FindCartridgeWithID
int CMacCmd::FindCartridgeWithID(void)
{
  int slot = mScope->FindCartridgeWithID(mItemInt[1], mStrCopy);
  if (slot < 0)
    ABORT_LINE(mStrCopy + " for line:\n\n");
  mLogRpt.Format("ID %d is at inventory array index # %d", mItemInt[1], slot);
  SetRepValsAndVars(1, (double)slot);
  return 0;
}

// LoadCartridge, UnloadCartridge
int CMacCmd::LoadCartridge(void)
{
  int index;

  if (CMD_IS(LOADCARTRIDGE))
    index = mScope->LoadCartridge(mItemInt[1]);
  else
    index = mScope->UnloadCartridge();
  if (index) {
    if (index == 1)
      ABORT_LINE("The thread is already busy for a long operation in:\n\n");
    if (index == 3)
      ABORT_LINE("Autoloader operations are not supported by this microscope in:\n\n");
    if (index == 4)
      ABORT_LINE("Slot number or index is out of range in:\n\n");
    if (index == 6)
      ABORT_LINE("You need to do a cassette inventory; There is no cartridge "
        "information for:\n\n");
    if (index == 5) {
      ABORT_LINE((CMD_IS(LOADCARTRIDGE) ? "The cartridge is already in the stage" :
        "There is no cartridge in the stage") +
        CString(" according to the current inventory in:\n\n"));
    } else {
      ABORT_LINE("There was an error trying to run a long operation with:\n\n");
    }
  }
  mStartedLongOp = true;
  return 0;
}

// RefrigerantLevel
int CMacCmd::RefrigerantLevel(void)
{
  double delX;

  if (FEIscope && (mItemInt[1] < 1 || mItemInt[1] > 3))
    ABORT_LINE("Dewar number must be between 1 and 3 in: \n\n");
  if (JEOLscope && (mItemInt[1] < 0 || mItemInt[1] > 2))
    ABORT_LINE("Dewar number must be between 0 and 2 in: \n\n");
  if (!mScope->GetRefrigerantLevel(mItemInt[1], delX)) {
    if (JEOLscope && GetDebugOutput('1'))
      SEMMessageBox("Script halted due to failure in RefrigerantLevel");
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Refrigerant level in dewar %d is %.3f", mItemInt[1], delX);
  SetRepValsAndVars(2, delX);
  return 0;
}

// DewarsRemainingTime
int CMacCmd::DewarsRemainingTime(void)
{
  int index;

  if (!mScope->GetDewarsRemainingTime(0, index)) {
    if (JEOLscope && GetDebugOutput('1'))
      SEMMessageBox("Script halted due to failure in DewarsRemainingTime");
    AbortMacro();
    return 1;
  }
  if (index < 0) {
    if (mScope->GetHasSimpleOrigin())
      mLogRpt = index == -1 ? "There are no refills left" :
      "the SimpleOrigin system is not active";
    else
      mLogRpt = "No dewar refilling is scheduled";
  } else
    mLogRpt.Format("Remaining time until dewars start filling is %d sec", index);
  SetRepValsAndVars(1, (double)index);
  return 0;
}

// AreDewarsFilling
int CMacCmd::AreDewarsFilling(void)
{
  int index;

  if (!mScope->AreDewarsFilling(index)) {
    if (JEOLscope && GetDebugOutput('1'))
      SEMMessageBox("Script halted due to failure in AreDewarsFilling");
    AbortMacro();
    return 1;
  }
  if (mScope->GetHasSimpleOrigin()) {
    mLogRpt.Format("SimpleOrigin system %s busy filling", index ? "IS" : "is NOT");
  } else if (FEIscope)
    mLogRpt.Format("Dewars %s busy filling", index ? "ARE" : "are NOT");
  else {
    char *dewarTxt[4] = {"No tanks are", "Stage tank is", "Transfer tank is",
      "Stage and transfer tanks are"};
    B3DCLAMP(index, 0, 3);
    mLogRpt.Format("%s being refilled", dewarTxt[index]);
  }
  SetRepValsAndVars(1, index);
  return 0;
}

// SimpleOriginStatus
int CMacCmd::SimpleOriginStatus(void)
{
  int numLeft, secToNext, active, filling;
  float sensor;
  if (!mScope->GetSimpleOriginStatus(numLeft, secToNext, filling, active, &sensor)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("SimpleOrigin has %d refills, %.1f minutes to next fill, %s filling, "
    "%s active, sensor %.1f C", numLeft, secToNext / 60., filling ? "IS" : "is NOT", 
    active ? "IS" : "is NOT", sensor);
  SetRepValsAndVars(1, numLeft, secToNext / 60., filling, active);
  return 0;
}

// SetSimpleOriginActive
int CMacCmd::SetSimpleOriginActive(void)
{
  if (!mScope->SetSimpleOriginActive(mItemInt[1], mStrCopy))
    ABORT_NOLINE(mStrCopy);
  return 0;
}

// ManageDewarsAndPumps
int CMacCmd::ManageDewarsAndPumps(void)
{
  int checks = mItemEmpty[1] ? 0 : mItemInt[1];
  DewarVacParams *param = mScope->GetDewarVacParams();
  if (mWinApp->mParticleTasks->ManageDewarsVacuum(*param, checks))
    ABORT_LINE("Script halted due to failure to start a long operation for line:\n\n");
  return 0;
}

// ReportVacuumGauge
int CMacCmd::ReportVacuumGauge(void)
{
  CString report;
  double delISX;
  int index;

  if (!mScope->GetGaugePressure((LPCTSTR)mStrItems[1], index, delISX)) {
    if (JEOLscope && index < -1 && index > -4) {
      report.Format("Name must be \"Pir\" or \"Pen\" followed by number between 0 and "
        "9 in line:\n\n");
      ABORT_LINE(report);
    }
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Gauge %s status is %d, pressure is %f", (LPCTSTR)mStrItems[1], index,
    delISX);
  SetRepValsAndVars(2, index, delISX);
  return 0;
}

// ReportVacuumStatus
int CMacCmd::ReportVacuumStatus(void)
{
  mLogRpt.Format("Vacuum status index is %d", mWinApp->mScopeStatus.GetVacStatus());
  SetRepValsAndVars(1, mWinApp->mScopeStatus.GetVacStatus());
  return 0;
}

// ReportHighVoltage
int CMacCmd::ReportHighVoltage(void)
{
  double delISX;

  delISX = mScope->GetHTValue();
  mLogRpt.Format("High voltage is %.1f kV", delISX);
  SetRepValsAndVars(1, delISX);
  return 0;
}

// SetSlitWidth
int CMacCmd::SetSlitWidth(void)
{
  double delISX;

  if (mItemEmpty[1])
    ABORT_LINE("SetSlitWidth must be followed by a number in: \n\n");
  delISX = mItemDbl[1];
  if (delISX < mFiltParam->minWidth || delISX > mFiltParam->maxWidth)
    ABORT_LINE("This is not a legal slit width in: \n\n");
  mFiltParam->slitWidth = (float)delISX;
  mWinApp->mFilterControl.UpdateSettings();
  mCamera->SetupFilter();
  UpdateLDAreaIfSaved();
  return 0;
}

// SetEnergyLoss, ChangeEnergyLoss
int CMacCmd::SetEnergyLoss(void)
{
  CString report;
  double delISX, delISY;

  if (mItemEmpty[1])
    ABORT_LINE(mStrItems[0] + " must be followed by a number in: \n\n");
  delISX = mItemDbl[1];
  if (CMD_IS(CHANGEENERGYLOSS))
    delISX += mFiltParam->energyLoss;
  if (mWinApp->mFilterControl.LossOutOfRange(delISX, delISY)) {
    report.Format("The energy loss requested in:\n\n%s\n\nrequires a net %s of %.1f"
      "with the current adjustments.\nThis net value is beyond the allowed range.",
      (LPCTSTR)mStrLine, mScope->GetHasOmegaFilter() ? "shift" : "offset", delISY);
    ABORT_LINE(report);
  }
  mFiltParam->energyLoss = (float)delISX;
  mFiltParam->zeroLoss = false;
  mWinApp->mFilterControl.UpdateSettings();
  mCamera->SetupFilter();
  UpdateLDAreaIfSaved();
  return 0;
}

// SetSlitIn
int CMacCmd::SetSlitIn(void)
{
  int index;

  index = 1;
  if (!mItemEmpty[1])
    index = mItemInt[1];
  mFiltParam->slitIn = index != 0;
  mWinApp->mFilterControl.UpdateSettings();
  mCamera->SetupFilter();
  UpdateLDAreaIfSaved();
  return 0;
}

// RefineZLP
int CMacCmd::RefineZLP(void)
{
  CString report;

  if (mItemEmpty[1] || !mItemDbl[1] || 
    SEMTickInterval(1000. * mFiltParam->alignZLPTimeStamp) > 60000. *mItemDbl[1]) {
    CTime ctdt = CTime::GetCurrentTime();
    report.Format("%02d:%02d:%02d", ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond());
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    mWinApp->mFilterTasks->RefineZLP(false, mItemInt[2]);
  }
  return 0;
}

// SelectCamera
int CMacCmd::SelectCamera(void)
{
  int index;

  index = mItemInt[1];
  if (index < 1 || index > mWinApp->GetNumActiveCameras())
    ABORT_LINE("Camera number out of range in: \n\n");
  RestoreCameraSet(-1, true);
  mWinApp->SetActiveCameraNumber(index - 1);
  return 0;
}

// ReportExposure
int CMacCmd::ReportExposure(void)
{
  int index;
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  mLogRpt.Format("%s exposure is %.3f, drift settling %.3f", mModeNames[index], 
    mConSets[index].exposure, mConSets[index].drift);
  SetRepValsAndVars(2, mConSets[index].exposure, mConSets[index].drift);
  return 0;
}

// ReportBinning
int CMacCmd::ReportBinning(void)
{
  int index;
  float value;
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  value = (float)mConSets[index].binning / BinDivisorF(mCamParams);
  mLogRpt.Format("%s binning is %g", mModeNames[index], value);
  SetRepValsAndVars(2, value);
  return 0;
}

// ReportReadMode
int CMacCmd::ReportReadMode(void)
{
  int index;
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  mLogRpt.Format("%s read mode is %d", mModeNames[index], mConSets[index].K2ReadMode);
  SetRepValsAndVars(2, mConSets[index].K2ReadMode);
  return 0;
}

// ReportCameraSetArea
int CMacCmd::ReportCameraSetArea(void)
{
  int index, left, right, bottom, top, sizeX, sizeY, div = BinDivisorI(mCamParams);
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  left = mConSets[index].left;
  right = mConSets[index].right;
  top = mConSets[index].top;
  bottom = mConSets[index].bottom;
  mCamera->AcquiredSize(&mConSets[index], mCurrentCam, sizeX, sizeY);
  mLogRpt.Format("%s is %d x %d binned pixels (unbinned l %d r %d, t %d b %d)",
    mModeNames[index], sizeX, sizeY, left / div, right / div, top /div, bottom / div);
  SetRepValsAndVars(2, sizeX, sizeY, left / div, right / div, top / div, 
    bottom / div);
  return 0;
}

// ReportCurrentPixelSize
int CMacCmd::ReportCurrentPixelSize(void)
{
  int index;
  float value;
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  value = mShiftManager->GetPixelSize(mCurrentCam, mScope->FastMagIndex()) *
    (float)mConSets[index].binning * 1000.f;
  mLogRpt.Format("%s pixel size is %.4g nm", mModeNames[index], value);
  SetRepValsAndVars(2, value);
  return 0;
}

// SetExposure
int CMacCmd::SetExposure(void)
{
  double delX, delY;
  int index;

  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  delX = mItemDbl[2];
  delY = mItemEmpty[3] ? 0. : mItemDbl[3];
  if (mItemEmpty[2] || delX <= 0. || delY < 0.)
    ABORT_LINE("Incorrect entry for setting exposure: \n\n");
  SaveControlSet(index);
  mConSets[index].exposure = (float)delX;
  if (!mItemEmpty[3])
    mConSets[index].drift = (float)delY;
  return 0;
}

// SetBinning
int CMacCmd::SetBinning(void)
{
  int index, index2;

  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  if (CheckCameraBinning(mItemDbl[2], index2, mStrCopy))
    ABORT_LINE(mStrCopy);
  SaveControlSet(index);
  mConSets[index].binning = index2;
  return 0;
}

// SetCameraArea
int CMacCmd::SetCameraArea(void)
{
  CString report;
  int index, index2, ix0, ix1, iy0, iy1, sizeX, sizeY;

  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  report = mStrItems[2];
  report.MakeUpper();
  if (report == "F" || report == "H" || report == "Q" ||
    report == "WH" || report == "WQ") {
    if (report == "F" || report == "WH") {
      ix0 = 0;
      ix1 = mCamParams->sizeX;
    } else if (report == "Q") {
      ix0 = 3 * mCamParams->sizeX / 8;
      ix1 = 5 * mCamParams->sizeX / 8;
    } else {
      ix0 = mCamParams->sizeX / 4;
      ix1 = 3 * mCamParams->sizeX / 4;
    }
    if (report == "F") {
      iy0 = 0;
      iy1 = mCamParams->sizeY;
    } else if (report == "H" || report == "WH") {
      iy0 = mCamParams->sizeY / 4;
      iy1 = 3 * mCamParams->sizeY / 4;
    } else {
      iy0 = 3 * mCamParams->sizeY / 8;
      iy1 = 5 * mCamParams->sizeY / 8;
    }
  } else {
    if (mItemEmpty[5])
      ABORT_LINE("Not enough coordinates for setting camera area: \n\n");
    index2 = BinDivisorI(mCamParams);
    ix0 = B3DMAX(0, B3DMIN(mCamParams->sizeX - 4, index2 * mItemInt[2]));
    ix1 = B3DMAX(ix0 + 1, B3DMIN(mCamParams->sizeX, index2 * mItemInt[3]));
    iy0 = B3DMAX(0, B3DMIN(mCamParams->sizeY - 4, index2 * mItemInt[4]));
    iy1 = B3DMAX(iy0 + 1, B3DMIN(mCamParams->sizeY, index2 * mItemInt[5]));
  }
  index2 = mConSets[index].binning;
  iy0 /= index2;
  iy1 /= index2;
  ix0 /= index2;
  ix1 /= index2;
  sizeX = ix1 - ix0;
  sizeY = iy1 - iy0;
  mCamera->AdjustSizes(sizeX, mCamParams->sizeX, mCamParams->moduloX, ix0, ix1, sizeY,
    mCamParams->sizeY, mCamParams->moduloY, iy0, iy1, index2);
  SaveControlSet(index);
  mConSets[index].left = ix0 * index2;
  mConSets[index].right = ix1 * index2;
  mConSets[index].top = iy0 * index2;
  mConSets[index].bottom = iy1 * index2;
  return 0;
}

// SetCenteredSize
int CMacCmd::SetCenteredSize(void)
{
  int index, index2, ix0, ix1, iy0, iy1, sizeX, sizeY;

  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  if (CheckCameraBinning(mItemDbl[2], index2, mStrCopy))
    ABORT_LINE(mStrCopy);
  sizeX = mItemInt[3];
  sizeY = mItemInt[4];
  if (sizeX < 4 || sizeX * index2 > mCamParams->sizeX || sizeY < 4 ||
    sizeY *index2 > mCamParams->sizeY)
    ABORT_LINE("Image size is out of range for current camera at given binning in:"
    " \n\n");
  SaveControlSet(index);
  mConSets[index].binning = index2;
  mCamera->CenteredSizes(sizeX, mCamParams->sizeX, mCamParams->moduloX, ix0, ix1,
    sizeY, mCamParams->sizeY, mCamParams->moduloY, iy0, iy1, index2);
  mConSets[index].left = ix0 * index2;
  mConSets[index].right = ix1 * index2;
  mConSets[index].top = iy0 * index2;
  mConSets[index].bottom = iy1 * index2;
  return 0;
}

// SetExposureForMean
int CMacCmd::SetExposureForMean(void)
{
  double delISX, delISY;
  int index, index2, ix1, iy1;
  float bmin, bmean;

  index = RECORD_CONSET;
  if (!mImBufs->mImage || mImBufs->IsProcessed() ||
    (mWinApp->LowDoseMode() && mCamera->ConSetToLDArea(mImBufs->mConSetUsed) != index))
    ABORT_LINE("Script aborted setting exposure time because\n"
    " there is no image or a processed image in Buffer A\n"
    "(or, if in Low Dose mode, because the image in\n"
    " Buffer A is not from the Record area) for line:\n\n");
  if (mItemDbl[1] <= 0)
    ABORT_LINE("Exposure time must be positive for line:\n\n");
  delISX = mItemDbl[1] / mProcessImage->EquivalentRecordMean(0);
  delISY = delISX * B3DMAX(0.001, mConSets[index].exposure - mCamParams->deadTime) +
    mCamParams->deadTime;
  if (mItemInt[2]) {

    // Adjusting frame time to keep constant number of frames
    if (!mCamParams->K2Type || !mConSets[index].doseFrac)
      ABORT_LINE("Frame time can be adjusted only for K2/K3 camera"
      " with dose fractionation mode on in line:\n\n");
    index2 = B3DNINT(mConSets[index].exposure /
      B3DMAX(0.001, mConSets[index].frameTime));
    bmin = (float)(delISY / index2);
    mCamera->ConstrainFrameTime(bmin, mCamParams);
    if (fabs(bmin - mConSets[index].frameTime) < 0.0001) {
      PrintfToLog("In SetExposureForMean %s, change by a factor of %.4f would require "
        "too small a change in frame time", (LPCTSTR)mStrItems[1], delISX);
      bmean = 0.;
    } else {
      SaveControlSet(index);
      mConSets[index].frameTime = bmin;
      bmean = index2 * bmin;
      PrintfToLog("In SetExposureForMean %s, frame time changed to %.4f, exposure time"
        " changed to %.4f", (LPCTSTR)mStrItems[1], mConSets[index].frameTime,
        bmean);
    }
  } else {

    // Just adjusting exposure time
    bmean = (float)delISY;
    ix1 = mCamera->DESumCountForConstraints(mCamParams, &mConSets[index]);
    mCamera->CropTietzSubarea(mCamParams, mConSets[index].right - mConSets[index].left,
      mConSets[index].bottom - mConSets[index].top, mConSets[index].processing,
      mConSets[index].mode, iy1);
    mCamera->ConstrainExposureTime(mCamParams, mConSets[index].doseFrac,
      mConSets[index].K2ReadMode, mConSets[index].binning,
      mCamera->MakeAlignSaveFlags(&mConSets[index]), ix1, bmean,
      mConSets[index].frameTime, iy1, mConSets[index].mode);
    if (fabs(bmean - mConSets[index].exposure) < 0.00001) {
      PrintfToLog("In SetExposureForMean %s, change by a factor of %.4f would require "
        "too small a change in exposure time", (LPCTSTR)mStrItems[1], delISX);
      bmean = 0.;
    } else {
      SaveControlSet(index);
      bmean = mWinApp->mFalconHelper->AdjustSumsForExposure(mCamParams,
        &mConSets[index], bmean);
      /* This is good for seeing how the distribute frames function works
      report.Format("Summed frame list:");
      for (index2 = 0; index2 < mConSets[index].summedFrameList.size(); index2++) {
        strCopy.Format(" %d", mConSets[index].summedFrameList[index2]);
        report+= strCopy;
      }
      mWinApp->AppendToLog(report); */
      PrintfToLog("In SetExposureForMean %s, exposure time changed to %.4f",
        (LPCTSTR)mStrItems[1], bmean);
    }
  }

  // Commit the new exposure and report if change is not good
  if (bmean) {
    float diffThresh = 0.05f;
    mConSets[index].exposure = bmean;
    if (fabs(bmean - delISY) / delISY > diffThresh)
      PrintfToLog("WARNING: Desired exposure time (%.3f) differs from actual one "
      "(%.3f) by more than %d%%", delISY, bmean, B3DNINT(100. * diffThresh));
  }
  return 0;
}

// SetContinuous
int CMacCmd::SetContinuous(void)
{
  int index;

  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  SaveControlSet(index);
  mConSets[index].mode = mItemInt[2] ? CONTINUOUS : SINGLE_FRAME;
  return 0;
}

// SetProcessing
int CMacCmd::SetProcessing(void)
{
  int index;

  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  if (mItemInt[2] < 0 || mItemInt[2] > 2)
    ABORT_LINE("Processing must 0, 1, or 2 in:\n\n");
  SaveControlSet(index);
  mConSets[index].processing = mItemInt[2];
  return 0;
}

// SetFrameTime, ChangeFrameAndExposure
int CMacCmd::SetFrameTime(void)
{
  BOOL truth;
  int index, ix0, iy1, newSum, subframes;
  float fps, exposure;
  int *sumP;
  bool savingInEER, alignInFalcon, falconCanSave = false;

  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  if (mCamParams->FEItype) {
    mCamera->CanWeAlignFalcon(mCamParams, true, falconCanSave,
      mCamSet->K2ReadMode);
    savingInEER = mCamera->IsSaveInEERMode(mCamParams, mCamSet);
    alignInFalcon = IS_FALCON3_OR_4(mCamParams) && FCAM_CAN_ALIGN(mCamParams) &&
      mCamSet->alignFrames && !mCamSet->useFrameAlign;
    if (!(IS_FALCON3_OR_4(mCamParams) || mCamera->GetMaxFalconFrames() > 7))
      falconCanSave = false;
  }
  if (!mCamParams->K2Type && !mCamParams->canTakeFrames &&
    !falconCanSave && !mWinApp->mDEToolDlg.CanSaveFrames(mCamParams))

    ABORT_NOLINE("Frame time cannot be set for the current camera type");
  SaveControlSet(index);
  truth = CMD_IS(CHANGEFRAMEANDEXPOSURE);

  // Falcon is handled separately for fractions
  if (falconCanSave && !savingInEER) {
    exposure = (truth ? mItemFlt[2] : 1.f) * mCamSet->exposure;
    subframes = B3DNINT(exposure / mCamera->GetFalconFractionDivisor(mCamParams)
      - (mCamSet->numSkipBefore + mCamSet->numSkipAfter));
    if (truth && !mCamSet->summedFrameList.size()) {
      mCamSet->userFrameFractions.resize(1);
      mCamSet->userFrameFractions[0] = 1.;
      mCamSet->userSubframeFractions.resize(1);
      mCamSet->userSubframeFractions[0] = 1.;
    }
    if (truth) {
      ix0 = mWinApp->mFalconHelper->GetFrameTotals(mCamSet->summedFrameList, subframes);
      mCamSet->exposure = exposure;
    } else
      ix0 = B3DNINT(exposure / mItemFlt[2]);
    mWinApp->mFalconHelper->DistributeSubframes(mCamSet->summedFrameList, subframes, ix0, 
      mCamSet->userFrameFractions, mCamSet->userSubframeFractions, alignInFalcon);
    ix0 = mWinApp->mFalconHelper->GetFrameTotals(mCamSet->summedFrameList, subframes);
    SetRepValsAndVars(3, ix0, exposure / ix0, exposure);
    if (mCamParams->FEItype != FALCON4_TYPE)
      return 0;

    // DE camera is handled separately
  } else if (mCamParams->DE_camType) {
    fps = mCamParams->DE_FramesPerSec;
    if (mCamSet->K2ReadMode > 0 && mCamParams->DE_CountingFPS > 0.)
      fps = mCamParams->DE_CountingFPS;
    sumP = mCamSet->K2ReadMode > 0 ? &mCamSet->sumK2Frames : &mCamSet->DEsumCount;
    if (truth) {
      newSum = B3DNINT(mItemFlt[2] * *sumP);
      mCamSet->exposure *= (float)newSum / (float)(*sumP);
      *sumP = newSum;
    } else {
      *sumP = B3DMAX(1, B3DNINT(fps * mItemFlt[2]));
      ix0 = B3DNINT(mCamSet->exposure / (*sumP / fps));
      mCamSet->exposure = B3DMAX(1, ix0) * (*sumP / fps);
    }
    SetRepValsAndVars(3, *sumP / fps, mCamSet->exposure);
    mLogRpt.Format("New frame time %.3f  exposure %.3f", *sumP / fps, mCamSet->exposure);
    return 0;
  }

  // Other cameras
  if (truth) {
    ix0 = B3DNINT(mCamSet->exposure / mCamSet->frameTime);
    mCamSet->frameTime *= mItemFlt[2];
  } else {
    mCamSet->frameTime = mItemFlt[2];
  }
  mCamera->CropTietzSubarea(mCamParams, mCamSet->right - mCamSet->left,
    mCamSet->bottom - mCamSet->top, mCamSet->processing,
    mCamSet->mode, iy1);
  mCamera->ConstrainFrameTime(mCamSet->frameTime, mCamParams,
    mCamSet->binning, mCamParams->OneViewType ? mCamSet->K2ReadMode:iy1);
  if ((!falconCanSave || savingInEER) && truth) {
    mCamSet->exposure = mCamSet->frameTime * ix0;
    SetRepValsAndVars(3, mCamSet->frameTime, mCamSet->exposure);
  } else if (!falconCanSave || savingInEER) {
    SetRepValsAndVars(3, mCamSet->frameTime);
  }
  return 0;
}

// SetK2ReadMode
int CMacCmd::SetK2ReadMode(void)
{
  int index;

  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  if (!mCamParams->K2Type && !mCamParams->OneViewType && !IS_FALCON3_OR_4(mCamParams) &&
    !mCamParams->DE_camType)
    ABORT_NOLINE("Read mode cannot be set for the current camera type");
  if (mItemInt[2] < 0 || mItemInt[2] > 2)
    ABORT_LINE("Read mode must 0, 1, or 2 in:\n\n");
  SaveControlSet(index);
  mConSets[index].K2ReadMode = mItemInt[2];
  return 0;
}

// SetDoseFracParams
int CMacCmd::SetDoseFracParams(void)
{
  int index;

  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  if (!mItemEmpty[5] && (mItemInt[5] < 0 || mItemInt[5] > 2))
    ABORT_LINE("Alignment method (the fourth number) must be 0, 1, or 2 in:\n\n");
  SaveControlSet(index);
  mConSets[index].doseFrac = mItemInt[2] ? 1 : 0;
  if (!mItemEmpty[3])
    mConSets[index].saveFrames = mItemInt[3] ? 1 : 0;
  if (!mItemEmpty[4])
    mConSets[index].alignFrames = mItemInt[4] ? 1 : 0;
  if (!mItemEmpty[5])
    mConSets[index].useFrameAlign = mItemInt[5];
  if (!mItemEmpty[6])
    mConSets[index].sumK2Frames = mItemInt[6] ? 1 : 0;
  return 0;
}

// SetDECamFrameRate
int CMacCmd::SetDECamFrameRate(void)
{
  CString report;
  double delISX;

  if (!(mCamParams->DE_camType && mCamParams->DE_FramesPerSec > 0))
    ABORT_LINE("The current camera must be a DE camera with adjustable frame rate for"
      " line:\n\n");
  delISX = mItemDbl[1];
  if (delISX < mCamParams->DE_MaxFrameRate + 1.)
    delISX = B3DMIN(delISX, mCamParams->DE_MaxFrameRate);
  if (delISX <= 0. || delISX > mCamParams->DE_MaxFrameRate) {
    report.Format("The new frame rate must be greater than zero\n"
      "and less than %.2f FPS for line:\n\n", mCamParams->DE_MaxFrameRate);
    ABORT_LINE(report);
  }
  if (mDEframeRateToRestore < 0) {
    mNumStatesToRestore++;
    mDEframeRateToRestore = mCamParams->DE_FramesPerSec;
    mDEcamIndToRestore = mCurrentCam;
  }
  mCamParams->DE_FramesPerSec = (float)delISX;
  mWinApp->mDEToolDlg.UpdateSettings();
  SetReportedValues(mDEframeRateToRestore);
  mLogRpt.Format("Changed frame rate of DE camera from %.2f to %.2f",
    mDEframeRateToRestore, delISX);
  return 0;
}

// UseContinuousFrames
int CMacCmd::UseContinuousFrames(void)
{
  BOOL truth;

  truth = mItemInt[1] != 0;
  if ((mUsingContinuous ? 1 : 0) != (truth ? 1 : 0))
    mCamera->ChangePreventToggle(truth ? 1 : -1);
  mUsingContinuous = mItemInt[1] != 0;
  return 0;
}

// StopContinuous
int CMacCmd::StopContinuous(void)
{
  mCamera->StopCapture(0);
  return 0;
}

// ReportContinuous
int CMacCmd::ReportContinuous(void)
{
  int index;

  index = mCamera->DoingContinuousAcquire();
  if (index)
    mLogRpt.Format("Continuous acquire is running with set %d", index - 1);
  else
    mLogRpt = "Continuous acquire is not running";
  SetRepValsAndVars(1, index - 1.);
  return 0;
}

// StartFrameWaitTimer
int CMacCmd::StartFrameWaitTimer(void)
{
  if (mItemInt[1] < 0)
    mFrameWaitStart = -2.;
  else
    mFrameWaitStart = GetTickCount();
  return 0;
}

// WaitForNextFrame
int CMacCmd::WaitForNextFrame(void)
{
  if (!mItemEmpty[1])
    mCamera->AlignContinuousFrames(mItemInt[1], mItemInt[2] != 0);
  mCamera->SetTaskFrameWaitStart((mFrameWaitStart >= 0 || mFrameWaitStart < -1.1) ?
    mFrameWaitStart : (double)GetTickCount());
  mFrameWaitStart = -1.;
  return 0;
}

// SetLiveSettleFraction
int CMacCmd::SetLiveSettleFraction(void)
{
  mCamera->SetContinuousDelayFrac((float)B3DMAX(0., mItemDbl[1]));
  return 0;
}

// SetSTEMDetectors
int CMacCmd::SetSTEMDetectors(void)
{
  int index, index2, ix0, iy0, sizeX;

  if (!mCamParams->STEMcamera)
    ABORT_LINE("The current camera is not a STEM camera in: \n\n");
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index, mStrCopy))
    ABORT_LINE(mStrCopy);
  sizeX = mCamera->GetMaxChannels(mCamParams);
  if (mItemEmpty[sizeX + 1])
    ABORT_LINE("There must be a detector number for each channel in: \n\n");
  index2 = 0;
  for (ix0 = 2; ix0 < sizeX + 2; ix0++) {
    if (mItemInt[ix0] < -1 || mItemInt[ix0] >= mCamParams->numChannels)
      ABORT_LINE("Detector number out of range in: \n\n");
    for (iy0 = ix0 + 1; iy0 < sizeX + 2; iy0++) {
      if (mItemInt[ix0] == mItemInt[iy0] && mItemInt[ix0] != -1)
        ABORT_LINE("Duplicate detector number in: \n\n");
    }
    if (mItemInt[ix0] >= 0)
      index2++;
  }
  if (!index2)
    ABORT_LINE("There must be at least one detector listed in: \n\n");
  SaveControlSet(index);
  for (ix0 = 0; ix0 < sizeX; ix0++)
    mConSets[index].channelIndex[ix0] = mItemInt[ix0 + 2];
  return 0;
}

// RestoreCameraSetCmd
int CMacCmd::RestoreCameraSetCmd(void)
{
  int index;

  index = -1;
  if (!mItemEmpty[1] && CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index,
    mStrCopy))
      ABORT_LINE(mStrCopy);
  if (RestoreCameraSet(index, true))
    ABORT_NOLINE("No camera parameters were changed; there is nothing to restore");
  if (index == RECORD_CONSET && mAlignWholeTSOnly) {
    SaveControlSet(index);
    mConSets[index].alignFrames = 0;
  }
  return 0;
}

// KeepCameraSetChanges
int CMacCmd::KeepCameraSetChanges(void)
{
  int index, ix0;
  ControlSet *masterSets = mWinApp->GetCamConSets() +
    MAX_CONSETS *mCurrentCam;
  index = -1;
  if (!mItemEmpty[1] && CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], index,
    mStrCopy))
    ABORT_LINE(mStrCopy);
  for (ix0 = (int)mConsetNums.size() - 1; ix0 >= 0; ix0--) {
    if (mConsetNums[ix0] == index || index < 0) {
      masterSets[mConsetNums[ix0]] = mConSets[mConsetNums[ix0]];
      mConsetsSaved.erase(mConsetsSaved.begin() + ix0);
      mConsetNums.erase(mConsetNums.begin() + ix0);
    }
  }
  return 0;
}

// ReportK2FileParams
int CMacCmd::ReportK2FileParams(void)
{
  int index, index2, ix0, ix1, iy0, iy1;

  index = mCamera->GetK2SaveAsTiff();
  index2 = mCamera->GetSaveRawPacked();
  ix0 = mCamera->GetUse4BitMrcMode();
  ix1 = mCamera->GetSaveTimes100();
  iy0 = mCamera->GetSkipK2FrameRotFlip();
  iy1 = mCamera->GetOneK2FramePerFile();
  mLogRpt.Format("File type %s  raw packed %d  4-bit mode %d  x100 %d  Skip rot %d  "
    "file/frame %d", index > 1 ? "TIFF ZIP" : (index ? "MRC" : "TIFF LZW"), index2,
    ix0, ix1, iy0, iy1);
  SetRepValsAndVars(1, index, index2, ix0, ix1, iy0, iy1);
  return 0;
}

// SetK2FileParams
int CMacCmd::SetK2FileParams(void)
{
  if (mItemInt[1] < 0 || mItemInt[1] > 2)
    ABORT_LINE("File type (first number) must be 0, 1, or 2 in:\n\n");
  if (mItemInt[2] < 0 || mItemInt[2] > 3)
    ABORT_LINE("Saving raw packed (second number) must be 0, 1, 2, or 3 in:\n\n");
  mCamera->SetK2SaveAsTiff(mItemInt[1]);
  if (!mItemEmpty[2])
    mCamera->SetSaveRawPacked(mItemInt[2]);
  if (!mItemEmpty[3])
    mCamera->SetUse4BitMrcMode(mItemInt[3] ? 1 : 0);
  if (!mItemEmpty[4])
    mCamera->SetSaveTimes100(mItemInt[4] ? 1 : 0);
  if (!mItemEmpty[5])
    mCamera->SetSkipK2FrameRotFlip(mItemInt[5] ? 1 : 0);
  if (!mItemEmpty[6])
    mCamera->SetOneK2FramePerFile(mItemInt[6] ? 1 : 0);
  return 0;
}

// ReportFrameAliParams, SetFrameAliParams, ReportFrameAli2, SetFrameAli2
int CMacCmd::ReportFrameAliParams(void)
{
  double delX;
  int index, index2, ix1;
  CArray < FrameAliParams, FrameAliParams > *faParamArr = mCamera->GetFrameAliParams();
  FrameAliParams *faParam;
  BOOL *useGPUArr = mCamera->GetUseGPUforK2Align();
  index = mConSets[RECORD_CONSET].faParamSetInd;
  if (index < 0 || index >= (int)faParamArr->GetSize())
    ABORT_LINE("The frame alignment parameter set index for Record is out of range "
      "for:\n\n");
  faParam = faParamArr->GetData() + index;
  ix1 = mCamParams->useSocket ? 1 : 0;
  if (CMD_IS(REPORTFRAMEALIPARAMS)) {
    index = (faParam->doRefine ? 1 : -1) * faParam->refineIter;
    index2 = (faParam->useGroups ? 1 : -1) * faParam->groupSize;
    mLogRpt.Format("Frame alignment for Record has %s %d, keep precision %d"
      ", strategy %d, all-vs-all %d, refine %d, group %d",
      faParam->binToTarget ? "target" : "binning",
      faParam->binToTarget ? faParam->targetSize : faParam->aliBinning,
      faParam->keepPrecision, faParam->strategy,
      faParam->numAllVsAll, index, index2);
    SetRepValsAndVars(1, faParam->aliBinning, faParam->keepPrecision,
      faParam->strategy, faParam->numAllVsAll, index, index2);

  } else if (CMD_IS(SETFRAMEALIPARAMS)) {
    if (mItemInt[1] < 1 || (mItemInt[1] > 16 && mItemInt[1] < 100))
      ABORT_LINE("Alignment binning is out of range in:\n\n");
    if (mItemInt[1] > 16)
      faParam->targetSize = mItemInt[1];
    else
      faParam->aliBinning = mItemInt[1];
    faParam->binToTarget = mItemInt[1] > 16;
    if (!mItemEmpty[2])
      faParam->keepPrecision = mItemInt[2] > 0;
    if (!mItemEmpty[3]) {
      B3DCLAMP(mItemInt[3], 0, 3);
      faParam->strategy = mItemInt[3];
    }
    if (!mItemEmpty[4])
      faParam->numAllVsAll = mItemInt[4];
    if (!mItemEmpty[5]) {
      faParam->doRefine = mItemInt[5] > 0;
      faParam->refineIter = B3DABS(mItemInt[5]);
    }
    if (!mItemEmpty[6]) {
      faParam->useGroups = mItemInt[6] > 0;
      faParam->groupSize = B3DABS(mItemInt[6]);
    }

  } else if (CMD_IS(REPORTFRAMEALI2)) {                 // ReportFrameAli2
    delX = (faParam->truncate ? 1 : -1) * faParam->truncLimit;
    mLogRpt.Format("Frame alignment for Record has GPU %d, truncation %.2f, hybrid %d,"
      " filters %.4f %.4f %.4f", useGPUArr[ix1], delX, faParam->hybridShifts,
      faParam->rad2Filt1, faParam->rad2Filt2, faParam->rad2Filt3);
    SetRepValsAndVars(1, useGPUArr[0], delX, faParam->hybridShifts,
      faParam->rad2Filt1, faParam->rad2Filt2, faParam->rad2Filt3);

  } else {                                              // SetFrameAli2
    useGPUArr[ix1] = mItemInt[1] > 0;
    if (!mItemEmpty[2]) {
      faParam->truncate = mItemDbl[2] > 0;
      faParam->truncLimit = (float)B3DABS(mItemDbl[2]);
    }
    if (!mItemEmpty[3])
      faParam->hybridShifts = mItemInt[3] > 0;
    if (!mItemEmpty[4]) {
      faParam->rad2Filt1 = mItemFlt[4];
      faParam->rad2Filt2 = mItemEmpty[5] ? 0.f : mItemFlt[5];
      faParam->rad2Filt3 = mItemEmpty[6] ? 0.f : mItemFlt[6];
    }
  }
  return 0;
}

// SetFolderForFrames
int CMacCmd::SetFolderForFrames(void)
{
  int index;

  if (!(mCamera->IsDirectDetector(mCamParams) ||
    (mCamParams->canTakeFrames & FRAMES_CAN_BE_SAVED)))
    ABORT_LINE("Cannot save frames from the current camera for line:\n\n");
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  if (mCamParams->DE_camType || (mCamParams->FEItype && FCAM_ADVANCED(mCamParams))) {
    if (mStrCopy.FindOneOf("/\\") >= 0)
      ABORT_LINE("Only a single folder can be entered for this camera type in:\n\n");
  } else {
    index = mStrCopy.GetAt(0);
    if (!(((index == ' / ' || index == '\\') &&
      (mStrCopy.GetAt(1) == ' / ' || mStrCopy.GetAt(1) == '\\')) ||
      ((mStrCopy.GetAt(2) == ' / ' || mStrCopy.GetAt(2) == '\\') &&
        mStrCopy.GetAt(1) == ':'
        && ((index >= 'A' && index <= 'Z') || (index >= 'a' && index <= 'z')))))
      ABORT_LINE("A complete absolute path must be entered in:\n\n");
  }
  if (mCamParams->K2Type)
    mCamera->SetDirForK2Frames(mStrCopy);
  else if (mCamParams->DE_camType)
    mCamera->SetDirForDEFrames(mStrCopy);
  else if (mCamParams->FEItype)
    mCamera->SetDirForFalconFrames(mStrCopy);
  else
    mCamParams->dirForFrameSaving = mStrCopy;
  return 0;
}

// SkipFrameAliParamCheck
int CMacCmd::SkipFrameAliParamCheck(void)
{
  int index;

  index = mItemEmpty[1] ? 1 : mItemInt[1];
  mSkipFrameAliCheck = index > 0;
  return 0;
}

// ReportK3CDSmode
int CMacCmd::ReportK3CDSmode(void)
{
  int index;

  index = mCamera->GetUseK3CorrDblSamp() ? 1 : 0;
  mLogRpt.Format("CDS mode is %s", index ? "ON" : "OFF");
  SetRepValsAndVars(1, index);
  return 0;
}

// SetK3CDSmode
int CMacCmd::SetK3CDSmode(void)
{
  if (mItemEmpty[2] || !mItemInt[2]) {
    if (mK3CDSmodeToRestore < 0) {
      mNumStatesToRestore++;
      mK3CDSmodeToRestore = mCamera->GetUseK3CorrDblSamp() ? 1 : 0;
    }
  } else {
    if (mK3CDSmodeToRestore >= 0)
      mNumStatesToRestore--;
    mK3CDSmodeToRestore = -1;
  }
  mCamera->SetUseK3CorrDblSamp(mItemInt[1] != 0);
  return 0;
}

// ReportCountScaling
int CMacCmd::ReportCountScaling(void)
{
  double delX;
  int index;

  index = mCamera->GetDivideBy2();
  delX = mCamera->GetCountScaling(mCamParams);
  if (mCamParams->K2Type == K3_TYPE)
    delX = mCamParams->countsPerElectron;
  SetRepValsAndVars(1, index, delX);
  mLogRpt.Format("Division by 2 is %s; count scaling is %.3f", index ? "ON" : "OFF",
    delX);
  return 0;
}

// SetDivideBy2
int CMacCmd::SetDivideBy2(void)
{
  if ((mCamParams->CamFlags & CAMFLAG_NO_DIV_BY_2) != 0 && mItemInt[1])
    ABORT_LINE("The cuurent camera does not support division by 2 for:\n\n");
  mCamera->SetDivideBy2(mItemInt[1] != 0 ? 1 : 0);
  return 0;
}

// ReportNumFramesSaved
int CMacCmd::ReportNumFramesSaved(void)
{
  int index;

  index = mCamera->GetNumFramesSaved();
  SetRepValsAndVars(1, index);
  mLogRpt.Format("Number of frames saved was %d", index);
  return 0;
}

// CameraProperties
int CMacCmd::CameraProperties(void)
{
  double delX;
  int index, index2, ix1, iy1, actNum = mWinApp->GetCurrentActiveCamera() + 1;

  if (!mItemEmpty[1]) {
    if (mItemInt[1] < 1 || mItemInt[1] > mWinApp->GetActiveCamListSize())
      ABORT_LINE("Active camera number is out of range in:\n\n")
    mCamParams = mWinApp->GetCamParams() + mActiveList[mItemInt[1] - 1];
    actNum = mItemInt[1];
  }
  ix1 = BinDivisorI(mCamParams);
  index = mCamParams->sizeX / ix1;
  index2 = mCamParams->sizeY / ix1;
  if (mItemEmpty[2])
    iy1 = mScope->GetMagIndex();
  else
    iy1 = mItemInt[2];
  delX = 1000. * ix1 * mShiftManager->GetPixelSize(
    mItemEmpty[1] ? mCurrentCam : mActiveList[mItemInt[1] - 1], iy1);
  mLogRpt.Format("%s: size %d x %d    rotation/flip %d   pixel on chip %.1f   "
    "unbinned pixel %.3f nm at %dx",
    (LPCSTR)mCamParams->name, index, index2, mCamParams->rotationFlip,
    mCamParams->pixelMicrons *ix1, delX, MagForCamera(mCamParams, iy1));
  SetReportedValues((double)index, (double)index2,
    (double)mCamParams->rotationFlip, mCamParams->pixelMicrons * ix1, delX,
    (double)actNum);
  return 0;
}

// ReportCameraName
int CMacCmd::ReportCameraName(void)
{
  CString name = "NOCAM";
  CameraParameters *camParams = mWinApp->GetCamParams();
  int index = mItemInt[1];
  if (index <= 0) {
    mLogRpt = "The name of the current camera is ";
    index = mWinApp->GetCurrentActiveCamera() + 1;
  } else {
    mLogRpt.Format("The name of active camera # %d is ", index);
  }
  if (index <= mWinApp->GetNumActiveCameras()) {
    name = camParams[mActiveList[index - 1]].name;
    mLogRpt += name;
  } else
    mLogRpt.Format("There is no active camera # %d", index);
  SetOneReportedValue(&mStrItems[2], name, 1);
  return 0;
}

// ReportColumnOrGunValve
int CMacCmd::ReportColumnOrGunValve(void)
{
  int index;

  index = mScope->GetColumnValvesOpen();
  if (index == -2)
    ABORT_NOLINE("An error occurred getting the state of the column/gun valve");
  SetRepValsAndVars(1, index);
  mLogRpt.Format("Column/gun valve state is %d", index);
  return 0;
}

// SetColumnOrGunValve
int CMacCmd::SetColumnOrGunValve(void)
{
  if (mItemEmpty[1])
    ABORT_LINE("Entry requires a number for setting valve open or closed: \n\n");
  if (!mScope->SetColumnValvesOpen(mItemInt[1] != 0))
    ABORT_NOLINE(mItemInt[1] ? "An error occurred opening the valve" :
    "An error occurred closing the valve");
  return 0;
}

// ReportFilamentCurrent
int CMacCmd::ReportFilamentCurrent(void)
{
  double delISX;

  delISX = mScope->GetFilamentCurrent();
  if (delISX < 0)
    ABORT_NOLINE("An error occurred getting the filament current");
  SetRepValsAndVars(1, delISX);
  mLogRpt.Format("Filament current is %.5g", delISX);
  return 0;
}

// SetFilamentCurrent
int CMacCmd::SetFilamentCurrent(void)
{
  if (!mScope->SetFilamentCurrent(mItemDbl[1]))
    ABORT_NOLINE("An error occurred setting the filament current");
  return 0;
}

// ReportFEGEmissionState
int CMacCmd::ReportFEGEmissionState(void)
{
  int state;
  if (!mScope->GetEmissionState(state))
    ABORT_NOLINE("An error occurred getting the emission state");
  SetRepValsAndVars(1, state);
  mLogRpt.Format("FEG emission is %s",state ? "ON" : "OFF");
  return 0;
}

// SetFEGEmissionState
int CMacCmd::SetFEGEmissionState(void)
{
  if (!mScope->SetEmissionState(mItemInt[1]))
    ABORT_NOLINE("An error occurred setting the emission state");
  return 0;
}

// IsFEGFlashingAdvised
int CMacCmd::IsFEGFlashingAdvised(void)
{
  int answer;
  if (mScope->GetAdvancedScriptVersion() < ASI_FILTER_FEG_LOAD_TEMP)
    ABORT_NOLINE("The version of advanced scripting has not been identified as high "
      "enough to support FEG flashing");
  if (!mScope->GetIsFlashingAdvised(mItemInt[1], answer)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("%s FEG flashing %s advised", mItemInt[1] ? "High-T" : "Low-T", 
    answer ? "IS" : "is NOT");
  SetRepValsAndVars(2, answer);
  return 0;
}

// NextFEGFlashHighTemp
int CMacCmd::NextFEGFlashHighTemp(void)
{
  if (mScope->GetAdvancedScriptVersion() < ASI_FILTER_FEG_LOAD_TEMP)
    ABORT_NOLINE("The version of advanced scripting has not been identified as high "
      "enough to support FEG flashing");
  mScope->SetDoNextFEGFlashHigh(mItemEmpty[1] || mItemInt[1]);
  return 0;
}

// ReportFEGBeamCurrent
int CMacCmd::ReportFEGBeamCurrent(void)
{
  double current;
  if (!FEIscope || !mScope->GetScopeCanFlashFEG())
    ABORT_NOLINE("FEG Beam current is available only for a TFS/FEI scope that can flash "
      "the FEG");
  if (!mScope->GetFEGBeamCurrent(current))
    ABORT_NOLINE("An error occurred getting the FEG beam current");
  mLogRpt.Format("FEG beam current is %5g nA", current);
  SetRepValsAndVars(1, current);
  return 0;
}

// IsPVPRunning
int CMacCmd::IsPVPRunning(void)
{
  BOOL truth;

  if (!mScope->IsPVPRunning(truth))
    ABORT_NOLINE("An error occurred determining whether the PVP is running");
  mLogRpt.Format("The PVP %s running", truth ? "IS" : "is NOT");
  SetRepValsAndVars(1, truth ? 1. : 0.);
  return 0;
}

// SetBeamBlank
int CMacCmd::SetBeamBlank(void)
{
  int index;

  if (mItemEmpty[1])
    ABORT_LINE("Entry requires a number for setting blank on or off: \n\n");
  index = mItemInt[1];
  mScope->BlankBeam(index != 0);
  return 0;
}

// MoveToNavItem
int CMacCmd::MoveToNavItem(void)
{
  int index;
  ABORT_NONAV;
  index = -1;
  if (!mItemEmpty[1]) {
    index = mItemInt[1] - 1;
    if (index < 0)
      ABORT_LINE("The Navigator item index must be positive: \n\n");
  }
  if (mNavigator->MoveToItem(index))
    ABORT_NOLINE("Error moving to Navigator item");
  mMovedStage = true;
  return 0;
}

// AlignToTemplate
int CMacCmd::AlignToTemplate(void)
{
  NavAlignParams params = *(mNavHelper->GetNavAlignParams());
  ABORT_NONAV
  if (!mItemEmpty[1] && mItemFlt[1] >= 0.)
    params.maxAlignShift = mItemFlt[1];
  if (!mItemEmpty[2] && mItemFlt[2] >= 0.)
    params.resetISthresh = mItemFlt[2];
  if (!mItemEmpty[3] && mItemInt[3] >= 0)
    params.maxNumResetIS = mItemInt[3];
  if (!mItemEmpty[4] && mItemInt[4] >= 0)
    params.leaveISatZero = mItemInt[4] != 0;
  if (!mItemEmpty[5])
    JustStripItems(mStrLine, 5, params.templateLabel);
  if (mWinApp->mParticleTasks->AlignToTemplate(params))
    ABORT_NOLINE("Script halted due to failure in Align to Template routine");
  return 0;
}

// RealignToNavItem, RealignToOtherItem
int CMacCmd::RealignToNavItem(void)
{
  CString report;
  BOOL truth, justMove = false;
  int index, index2, ix0, ix1, iy0;
  float bmax;
  ABORT_NONAV;
  truth = CMD_IS(REALIGNTOOTHERITEM);
  index2 = truth ? 2 : 1;
  index = mItemInt[1] - 1;
  bmax = 0.;
  ix1 = ix0 = 0;
  if (!mItemEmpty[index2 + 2]) {
    if (mItemEmpty[index2 + 4])
      ABORT_LINE("Entry requires three values for controlling image shift reset in:"
      "\n\n");
    bmax = mItemFlt[index2 + 2];
    ix0 = mItemInt[index2 + 3];
    ix1 = mItemInt[index2 + 4];
    justMove = !mItemEmpty[index2 + 5] && mItemInt[index2 + 5] != 0;
  }
  if (!mItemEmpty[index2 + 1])
    mNavHelper->SetContinuousRealign(mItemInt[index2 + 1]);
  if (truth)
    iy0 = mNavigator->RealignToOtherItem(index, mItemInt[index2] != 0, bmax, ix0,
      ix1, justMove);
  else
    iy0 = mNavigator->RealignToCurrentItem(mItemInt[index2] != 0, bmax, ix0, ix1,
      justMove);
  mNavHelper->SetContinuousRealign(0);
  if (iy0) {
    report.Format("Script halted due to failure %d in Realign to Item routine", iy0);
    ABORT_NOLINE(report);
  }
  return 0;
}

// SkipZMoveNextNavRealign
int CMacCmd::SkipZMoveNextNavRealign(void)
{
  mNavHelper->SetRISkipNextZMove(mItemEmpty[1] || mItemInt[1]);
  return 0;
}

// RealignToMapDrawnOn
int CMacCmd::RealignToMapDrawnOn(void)
{
  CString report;
  int ix0;
  CMapDrawItem *navItem;

  navItem = CurrentOrIndexedNavItem(mItemInt[1], mStrLine);
  if (!navItem)
    return 1;
  if (!navItem->mDrawnOnMapID)
    ABORT_LINE("The specified item has no ID for being drawn on a map in line:\n\n");
  ix0 = mNavHelper->RealignToDrawnOnMap(navItem, mItemInt[2] != 0);
  if (ix0) {
    report.Format("Script halted due to failure %d in Realign to Item for line:\n\n",
      ix0);
    ABORT_LINE(report);
  }
  return 0;
}

// GetRealignToItemError
int CMacCmd::GetRealignToItemError(void)
{
  float backlashX, backlashY, bmin, bmax;
  ABORT_NONAV;
  mNavHelper->GetLastStageError(backlashX, backlashY, bmin, bmax);
  SetRepValsAndVars(1, backlashX, backlashY, bmin, bmax);
  return 0;
}

// ReportNavItem, ReportOtherItem, ReportNextNavAcqItem, LoadNavMap, LoadOtherMap
int CMacCmd::ReportNavItem(void)
{
  CString report;
  BOOL truth;
  int index, index2, ix0, ix1;
  CMapDrawItem *navItem;
  ABORT_NONAV;
  truth = CMD_IS(REPORTNEXTNAVACQITEM);
  if (CMD_IS(REPORTNAVITEM) || CMD_IS(LOADNAVMAP)) {
    index = mNavigator->GetCurrentOrAcquireItem(navItem);
    if (index < 0)
      ABORT_LINE("There is no current Navigator item for line:\n\n");
    index2 = 1;
  } else if (truth) {
    if (!mNavigator->GetAcquiring())
      ABORT_LINE("The Navigator must be acquiring for line:\n\n");
    navItem = mNavigator->FindNextAcquireItem(index);
    if (index < 0) {
      mWinApp->AppendToLog("There is no next item to be acquired", mLogAction);
      SetVariable("NAVINDEX", "-1", VARTYPE_REGULAR + VARTYPE_ADD_FOR_NUM, -1, false);
      SetReportedValues(-1, 0.);
    }
  } else {
    if (mItemInt[1] < 0) {
      index = mNavigator->GetNumNavItems() + mItemInt[1];
    } else
      index = mItemInt[1] - 1;
    navItem = mNavigator->GetOtherNavItem(index);
    if (!navItem)
      ABORT_LINE("Index is out of range in statement:\n\n");
    index2 = 2;
  }

  if (CMD_IS(REPORTNAVITEM) || CMD_IS(REPORTOTHERITEM) || (truth && index >= 0)) {
    mLogRpt.Format("%stem %d:  Stage: %.2f %.2f %2.f  Label: %s",
      truth ? "Next i" : "I", index + 1, navItem->mStageX, navItem->mStageY,
      navItem->mStageZ, (LPCTSTR)navItem->mLabel);
    if (!navItem->mNote.IsEmpty())
      mLogRpt += "\r\n    Note: " + navItem->mNote;
    SetReportedValues(index + 1., navItem->mStageX, navItem->mStageY,
      navItem->mStageZ, (double)navItem->mType);
    report.Format("%d", index + 1);
    SetVariable("NAVINDEX", report, VARTYPE_REGULAR + VARTYPE_ADD_FOR_NUM, -1, false);
    SetVariable("NAVLABEL", navItem->mLabel, VARTYPE_REGULAR, -1, false);
    SetVariable("NAVNOTE", navItem->mNote, VARTYPE_REGULAR, -1, false);
    SetVariable("NAVCOLOR", navItem->mColor, VARTYPE_REGULAR + VARTYPE_ADD_FOR_NUM, -1,
      false);
    SetVariable("NAVREGIS", navItem->mRegistration,
      VARTYPE_REGULAR + VARTYPE_ADD_FOR_NUM, -1, false);
    index = atoi(navItem->mLabel);
    report.Format("%d", index);
    SetVariable("NAVINTLABEL", report, VARTYPE_REGULAR + VARTYPE_ADD_FOR_NUM, -1, false);
    mNavHelper->GetNumHolesFromParam(ix0, ix1, index2);
    SetVariable("NAVNUMHOLES", navItem->mAcquire ?
      mNavHelper->GetNumHolesForItem(navItem, index2) : 0,
      VARTYPE_REGULAR + VARTYPE_ADD_FOR_NUM, -1, false);
    if (mNavigator->GetAcquiring()) {
      report.Format("%d", mNavigator->GetNumAcquired() + (truth ? 2 : 1));
      SetVariable("NAVACQINDEX", report, VARTYPE_REGULAR + VARTYPE_ADD_FOR_NUM, -1,
        false);
    }
  } else if (!truth) {
    if (navItem->IsNotMap())
      ABORT_LINE("The Navigator item is not a map for line:\n\n");
    if (ConvertBufferLetter(mStrItems[index2], mBufferManager->GetBufToReadInto(),
      false, ix0, report))
      ABORT_LINE(report);
    mNavigator->DoLoadMap(false, navItem, ix0);
    mLoadingMap = true;
  }
  return 0;
}

// ReportItemAcquire
int CMacCmd::ReportItemAcquire(void)
{
  int index;
  CMapDrawItem *navItem;

  index = mItemEmpty[1] ? 0 : mItemInt[1];
  navItem = CurrentOrIndexedNavItem(index, mStrLine);
  if (!navItem)
    return 1;
 mLogRpt.Format("Navigator item %d has Acquire %s", index + 1,
    (navItem->mAcquire == 0) ? "disabled" : "enabled");
  SetReportedValues(navItem->mAcquire);
  return 0;
}

// SetItemAcquire
int CMacCmd::SetItemAcquire(void)
{
  BOOL truth;
  int index;
  CMapDrawItem *navItem;

  index = mItemEmpty[1] ? 0 : mItemInt[1];
  navItem = CurrentOrIndexedNavItem(index, mStrLine);
  if (!navItem)
    return 1;
  if (mNavigator->GetAcquiring() && index >= mNavigator->GetAcquireIndex() &&
    index <= mNavigator->GetEndingAcquireIndex())
    ABORT_NOLINE("When the Navigator is acquiring, you cannot set an\n"
      "item to Acquire within the range still being acquired");
  truth = (mItemEmpty[2] || (mItemInt[2] != 0));
  if (truth && navItem->mTSparamIndex >= 0)
    ABORT_LINE("You cannot turn on Acquire for an item set for a tilt series for "
      "line:\n\n")
  navItem->mAcquire = truth;
  mNavigator->UpdateListString(index);
  mNavigator->Redraw();
  mLogRpt.Format("Navigator item %d Acquire set to %s", index + 1,
    truth ? "enabled" : "disabled");
  mNavigator->SetChanged(true);
  mLoopInOnIdle = !mNavigator->GetAcquiring();
  return 0;
}

// SkipNavPointsNearEdge
int CMacCmd::SkipNavPointsNearEdge(void)
{
  ABORT_NONAV;
  MapItemArray *itemArray = mNavigator->GetItemArray();
  CMapDrawItem *item;
  FloatVec xcen, ycen, xBord, yBord, dists;
  Variable *xvar = NULL, *yvar = NULL;
  int ind, vecInd = 0, numOff = 0;
  int numItems = (int)itemArray->GetSize();
  int indSt = mItemInt[1] - 1, indEnd = mItemInt[2] - 1;
  bool excludeOff = !mItemEmpty[4] && mItemInt[4];

  // Error checks
  if (indSt < 0 || indSt >= numItems || indEnd < 1 || indEnd >= numItems ||
    indSt > indEnd)
    ABORT_LINE("Navigator indexes out of range or in wrong order in line:\n\n");
  if (mItemFlt[3] <= 0.)
    ABORT_LINE("Distance from edge must be positive in line:\n\n");
  if (!mItemEmpty[5] && mItemEmpty[6])
    ABORT_LINE("X array variable entered without Y variable in line:\n\n");

  // Find variables
  if (!mItemEmpty[5]) {
    xvar = LookupVariable(mStrItems[5], ind);
    yvar = LookupVariable(mStrItems[6], ind);
    if (!xvar || !yvar)
      ABORT_LINE("Variable not defined in line:\n\n");
    if (xvar->rowsFor2d || yvar->rowsFor2d)
      ABORT_LINE("One of the variables has a 2-D array for line:\n\n");
  }

  // Get eligible points
  for (ind = indSt; ind <= indEnd; ind++) {
    item = itemArray->GetAt(ind);
    if (item->IsPoint() && (!excludeOff || item->mAcquire)) {
      xcen.push_back(item->mStageX);
      ycen.push_back(item->mStageY);
    }
  }

  // Skip if not enough points
  if (xvar && !xcen.size()) {
    PrintfToLog("No points %swithin specified range; nothing to do",
      excludeOff ? "marked for acquire " : "");
    return 0;
  }
  if (!xvar && xcen.size() < 4) {
    PrintfToLog("Only %d points %swithin specified range; nothing to do", xcen.size(),
      excludeOff ? "marked for acquire " : "");
    return 0;
  }

  // Get supplied border and check
  if (xvar) {
    FillVectorFromArrayVariable(&xBord, NULL, xvar);
    FillVectorFromArrayVariable(&yBord, NULL, yvar);
    if (xBord.size() < 3 || yBord.size() < 3)
      ABORT_LINE("Array variables must have at least 3 boundary points for line:\n\n");
    if (xBord.size() != yBord.size())
      ABORT_LINE("Array variables for boundary coordinates do not have the same number "
        "of values in line:\n\n")
  } else {
    xBord.resize(xcen.size());
    yBord.resize(xcen.size());
  }

  // Get distances
  dists.resize(xcen.size());
  if (CAutoContouringDlg::FindDistancesFromHull(xcen, ycen, xBord, yBord, 1., dists,
    xvar != NULL))
    ABORT_LINE("Error allocating contour for boundary points in line:\n\n");

  // Turn off acquire for near points
  for (ind = indSt; ind <= indEnd; ind++) {
    item = itemArray->GetAt(ind);
    if (item->IsPoint() && (!excludeOff || item->mAcquire)) {
      if (item->mAcquire && dists[vecInd] < mItemFlt[3]) {
        item->mAcquire = false;
        mNavigator->UpdateListString(ind);
        numOff++;
      }
      vecInd++;
    }
  }
  PrintfToLog("%d Acquire points too close to edge %swill be skipped", numOff, 
    xvar ? "or outside boundary " : "");
  if (numOff) {
    mNavigator->Redraw();
    mNavigator->SetChanged(true);
  }
  return 0;
}

// NavIndexWithLabel, NavIndexWithNote
int CMacCmd::NavIndexWithLabel(void)
{
  BOOL truth;
  int index;
  ABORT_NONAV;
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  truth = CMD_IS(NAVINDEXWITHNOTE);
  mNavigator->FindItemWithString(mStrCopy, truth);
  index = mNavigator->GetFoundItem() + 1;
  if (index > 0)
    mLogRpt.Format("Item with %s %s has index %d", truth ? "note" : "label",
      (LPCTSTR)mStrCopy, index);
  else
    mLogRpt.Format("No item has %s %s", truth ? "note" : "label", (LPCTSTR)mStrCopy);
  SetReportedValues(index);
  return 0;
}

// NavIndexItemDrawnOn
int CMacCmd::NavIndexItemDrawnOn(void)
{
  int index, index2;
  CMapDrawItem *navItem;

  index = mItemInt[1];
  navItem = CurrentOrIndexedNavItem(index, mStrLine);
  if (!navItem)
    return 1;
  index2 = 0;
  if (!navItem->mDrawnOnMapID) {
    mLogRpt.Format("Navigator item %d does not have an ID for being drawn on a map",
      index + 1);
  } else {
    navItem = mNavigator->FindItemWithMapID(navItem->mDrawnOnMapID, true);
    if (!navItem) {
      mLogRpt.Format("The map that navigator item %d was drawn on is no longer in the "
        "table", index + 1);
    } else {
      index2 = mNavigator->GetFoundItem() + 1;
      mLogRpt.Format("Navigator item %d was drawn on map item %d", index + 1, index2);
    }
  }
  SetReportedValues(index2);
  return 0;
}

// NavItemFileToOpen
int CMacCmd::NavItemFileToOpen(void)
{
  CString report;
  int index, result = 0;
  CMapDrawItem *navItem;

  index = mItemInt[1];
  navItem = CurrentOrIndexedNavItem(index, mStrLine);
  if (!navItem)
    return 1;
  report = navItem->mFileToOpen;
  if (report.IsEmpty()) {
    mLogRpt.Format("No file is set to be opened for Navigator item %d", index + 1);
    report = "0";
  } else {
    mLogRpt.Format("File to open at Navigator item %d is: %s", index + 1,
      (LPCTSTR)report);
    result = 1;
  }
  SetOneReportedValue(report, 1);
  SetOneReportedValue(result, 2);
  return 0;
}

// SetNavItemUserValue, ReportItemUserValue
int CMacCmd::SetNavItemUserValue(void)
{
  CString report;
  int index, index2;
  CMapDrawItem *navItem;

  index = mItemInt[1];
  navItem = CurrentOrIndexedNavItem(index, mStrLine);
  if (!navItem)
    return 1;
  index2 = mItemInt[2];
  if (index2 < 1 || index2 > MAX_NAV_USER_VALUES) {
    report.Format("The user value number must be between 1 and %d in line:\n\n",
      MAX_NAV_USER_VALUES);
    ABORT_LINE(report);
  }
  if (CMD_IS(SETNAVITEMUSERVALUE)) {
    SubstituteLineStripItems(mStrLine, 3, mStrCopy);
    mNavHelper->SetUserValue(navItem, index2, mStrCopy);
    mNavigator->SetChanged(true);
  } else {
    if (mNavHelper->GetUserValue(navItem, index2, mStrCopy)) {
      mLogRpt.Format("Navigator item %d has no user value # %d", index + 1, index2);
      mStrCopy = "none";
    } else {
      mLogRpt.Format("User value # %d for Navigator item %d is %s", index2, index + 1,
        (LPCTSTR)mStrCopy);
    }
    SetOneReportedValue(mStrCopy, 1);
  }
  return 0;
}

// DeleteNavigatorItem
int CMacCmd::DeleteNavigatorItem(void)
{
  int index;
  CMapDrawItem *navItem;

  index = mItemInt[1];
  navItem = CurrentOrIndexedNavItem(index, mStrLine);
  if (!navItem)
    return 1;
  mNavigator->ExternalDeleteItem(navItem, index);
  if (!mItemInt[2])
    mWinApp->mMainView->DrawImage();
  return 0;
}

// GetNavGroupStageCoords, GetNavGroupImageCoords
int CMacCmd::GetNavGroupStageCoords(void)
{
  CMapDrawItem *item;
  MapItemArray *itemArray;
  int ind, bufInd, varInd = 2, sizeX, sizeY;
  int numPts = 0;
  float xx, yy;
  CString xval, yval, zval, str;
  bool image = CMD_IS(GETNAVGROUPIMAGECOORDS);
  ABORT_NONAV;
  if (mItemInt[1] <= 0)
    ABORT_LINE("Group ID must be positive in line:\n\n");
  if (image) {
    if (ConvertBufferLetter(mStrItems[2], 0, true, bufInd, mStrCopy))
      ABORT_LINE(mStrCopy);
    if (!mWinApp->mMainView->GetMapItemsForImageCoords(&mImBufs[bufInd], true)) {
      mLogRpt.Format("Navigator items could not be transformed to %c", bufInd + 65);
      SetReportedValues(-1);
      return 0;
    }
    varInd = 3;
    mImBufs[bufInd].mImage->getSize(sizeX, sizeY);
  }
  itemArray = mNavigator->GetItemArray();
  for (ind = 0; ind < (int)itemArray->GetSize(); ind++) {
    item = itemArray->GetAt(ind);
    if (item->IsPoint() && item->mGroupID == mItemInt[1]) {
      if (image) {
        mWinApp->mMainView->GetItemImageCoords(&mImBufs[bufInd], item, xx, yy);
        if (xx < 0. || xx > sizeX || yy < 0. || yy > sizeY)
          continue;
      } else {
        xx = item->mStageX;
        yy = item->mStageY;
        if (!mItemEmpty[4]) {
          str.Format("%s%f", numPts ? "\n" : "", item->mStageZ);
          zval += str;
        }
      }
      str.Format("%s%f", numPts ? "\n" : "", xx);
      xval += str;
      str.Format("%s%f", numPts ? "\n" : "", yy);
      yval += str;
      numPts++;
    }
  }
  mLogRpt.Format("%d point positions found in group", numPts);
  if (numPts) {
    if (SetVariable(mStrItems[varInd].MakeUpper(), xval, VARTYPE_REGULAR, -1, false,
      &mStrCopy))
      ABORT_LINE(mStrCopy + "in line:\n\n");
    if (SetVariable(mStrItems[varInd + 1].MakeUpper(), yval, VARTYPE_REGULAR, -1, false,
      &mStrCopy))
      ABORT_LINE(mStrCopy + "in line:\n\n");
    if (!image && !mItemEmpty[4]) {
      if (SetVariable(mStrItems[4].MakeUpper(), zval, VARTYPE_REGULAR, -1, false,
        &mStrCopy))
        ABORT_LINE(mStrCopy + "in line:\n\n");
    }
  }
  SetReportedValues(numPts);
  return 0;
}

// AddImagePosAsNavPoint, AddImagePointsAsPolygon
int CMacCmd::AddImagePosAsNavPoint(void)
{
  CString report;
  int index, numX, retVal = 0;
  bool draw = true;
  float stageZ;
  float *xArray = NULL, *yArray = NULL;
  ABORT_NONAV;
  if (ConvertBufferLetter(mStrItems[1], 0, true, index, report))
    ABORT_LINE(report);
  if (mItemEmpty[4] || mItemFlt[4] < -900.) {
    if (!mImBufs[index].GetStageZ(stageZ))
      ABORT_LINE("The image buffer has no stage Z value for line:\n\n");
  } else {
    stageZ = mItemFlt[4];
  }
  if (CMD_IS(ADDIMAGEPOSASNAVPOINT)) {
    draw = mItemInt[6] == 0;
    if (mNavigator->AddImagePositionOnBuffer(&mImBufs[index], mItemFlt[2], mItemFlt[3],
      stageZ, mItemInt[5])) {
      report = "The image does not have adequate coordinate information to add a point";
      retVal = 1;
    }
  } else {
    draw = mItemInt[5] == 0;
    retVal = GetPairedFloatArrays(2, &xArray, &yArray, numX, report);
    if (!retVal && mNavigator->AddPolygonFromImagePositions(&mImBufs[index], xArray,
      yArray, numX, stageZ)) {
      report = "The image does not have adequate coordinate information to add a point";
      retVal = 1;
    }

    delete[] xArray, yArray;
  }
  if (retVal)
    ABORT_LINE(report + " in line:\n\n");
  if (draw)
    mWinApp->mMainView->DrawImage();
  SetReportedValues(mNavigator->GetNumberOfItems());
  return 0;
}

// AdjustStagePosForNav
int CMacCmd::AdjustStagePosForNav(void)
{
  float stageX = mItemFlt[1];
  float stageY = mItemFlt[2];
  double ISX = mItemEmpty[3] ? 0. : mItemDbl[3];
  double ISY = mItemEmpty[4] ? 0. : mItemDbl[4];
  int camera = mCurrentCam;
  int magInd = (mItemEmpty[6] || mItemInt[6] < 0) ? mScope->FastMagIndex() : mItemInt[6];
  float tilt = mItemEmpty[7] ? (float)mScope->FastTiltAngle() : mItemFlt[7];
  ABORT_NONAV;
  if (!mItemEmpty[5] && mItemInt[5] >= 0) {
    if (mItemInt[5] < 1 || mItemInt[5] > mWinApp->GetActiveCamListSize())
      ABORT_LINE("The active camera number is out of range in line:\n\n");
    camera = mActiveList[mItemInt[5] - 1];
  }
  mNavigator->ConvertIStoStageIncrement(magInd, camera, ISX, ISY, tilt, stageX, stageY);
  mLogRpt.Format("Adjusted stage position %.3f  %.3f", stageX, stageY);
  SetReportedValues(stageX, stageY);
  return 0;
}

// AddStagePosAsNavPoint, AddStagePointsAsPolygon
int CMacCmd::AddStagePosAsNavPoint(void)
{
  CString report;
  bool draw = true;
  float *xArray = NULL, *yArray = NULL;
  int numPts, retVal;
  ABORT_NONAV;
  if (CMD_IS(ADDSTAGEPOSASNAVPOINT)) {
    draw = mItemInt[5] == 0;
    mNavigator->AddItemFromStagePositions(&mItemFlt[1], &mItemFlt[2], 1, mItemFlt[3],
      mItemInt[4]);
  } else {
    draw = mItemInt[4] == 0;
    retVal = GetPairedFloatArrays(1, &xArray, &yArray, numPts, report);
    if (!retVal)
      mNavigator->AddItemFromStagePositions(xArray, yArray, numPts, mItemFlt[3], 0);
    delete[] xArray, yArray;
    if (retVal)
      ABORT_LINE(report + " in line:\n\n");
  }

  if (draw)
    mWinApp->mMainView->DrawImage();
  SetReportedValues(mNavigator->GetNumberOfItems());
  return 0;
}

// GetUniqueNavID
int CMacCmd::GetUniqueNavID(void)
{
  int ID;
  ABORT_NONAV;
  ID = mNavigator->MakeUniqueID();
  SetRepValsAndVars(1, ID);
  return 0;
}

// EndAcquireAtItems
int CMacCmd::EndAcquireAtItems(void)
{
  ABORT_NONAV;
  if (mNavigator->GetPausedAcquire())
    mNavigator->EndAcquireWithMessage();
  else if (mNavigator->GetAcquiring())
    mNavigator->SetAcquireEnded(1);
  else
    ABORT_LINE("Navigator acquisition has not been started for line:\n\n");
  return 0;
}

// SetMapAcquireState
int CMacCmd::SetMapAcquireState(void)
{
  CString report;
  CMapDrawItem *navItem;
  ABORT_NONAV;
  navItem = mNavigator->GetOtherNavItem(mItemInt[1] - 1);
  if (!navItem)
    ABORT_LINE("Index is out of range in statement:\n\n");
  if (navItem->IsNotMap()) {
    report.Format("Navigator item %d is not a map for line:\n\n", mItemInt[1]);
    ABORT_LINE(report);
  }
  if (mNavHelper->SetToMapImagingState(navItem, true, 
    (mWinApp->LowDoseMode() && navItem->mMapLowDoseConSet >= 0) ? -1 : 0))
    ABORT_LINE("Failed to set map imaging state for line:\n\n");
  return 0;
}

// RestoreState
int CMacCmd::RestoreState(void)
{
  CString report;
  int index;

  index = mNavHelper->GetTypeOfSavedState();
  if (index == STATE_NONE) {
    report.Format("Cannot Restore State: no state has been saved");
    if (mItemInt[1])
      ABORT_LINE(report + " for line:\n\n");
    mWinApp->AppendToLog(report, mLogAction);
  } else {
    if (index == STATE_MAP_ACQUIRE)
      mNavHelper->RestoreFromMapState();
    else {
      mNavHelper->RestoreSavedState();
      if (mNavHelper->mStateDlg)
        mNavHelper->mStateDlg->Update();
    }
    if (mNavHelper->mStateDlg) {
      mNavHelper->mStateDlg->DisableUpdateButton();
      mNavHelper->mStateDlg->SetCamOfSetState(-1);
    }
  }
  return 0;
}

// ForgetPriorState
int CMacCmd::ForgetPriorState(void)
{
  CString report;
  if (mNavHelper->GetTypeOfSavedState() == STATE_NONE) {
    report.Format("Cannot forget prior state: no state has been saved");
    if (mItemInt[1])
      ABORT_LINE(report + " for line:\n\n");
    mWinApp->AppendToLog(report, mLogAction);
  } else {
    if (mNavHelper->mStateDlg)
      mNavHelper->mStateDlg->OnButForgetState();
    else
      mNavHelper->ForgetSavedState();
  }
  return 0;
}

// GoToImagingState
int CMacCmd::GoToImagingState(void)
{
  int index;
  CString errStr;
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  if (!mNavHelper->mStateDlg) {
    errStr = "the state dialog is not open";
    index = -1;
  } else
    index = mNavHelper->mStateDlg->SetStateByNameOrNum(mStrCopy, errStr);
  if (index)
    mLogRpt = "Cannot set imaging state; " + errStr;
  SetReportedValues(index);
  return 0;
}

// OpenStateDialog
int CMacCmd::OpenImagingStateDialog(void)
{
  if (!mNavHelper->mStateDlg)
    mNavHelper->OpenStateDialog();
  return 0;
}

// ReportNumNavAcquire
int CMacCmd::ReportNumNavAcquire(void)
{
  int index, index2;

  mNavHelper->CountAcquireItems(0, -1, index, index2);
  if (index < 0)
    mLogRpt = "The Navigator is not open; there are no acquire items";
  else
    mLogRpt.Format("Navigator has %d Acquire items and %d Tilt Series items", index, 
      index2);
  SetRepValsAndVars(1, (double)index, (double)index2);
  return 0;
}

// ReportNumHoleAcquire
int CMacCmd::ReportNumHoleAcquire(void)
{
  int index, numHoles, numRec;
  int minHoles = mItemEmpty[1] ? 1 : B3DMAX(1, mItemInt[1]);
  int startInd = mItemEmpty[2] ? 0 : (mItemInt[2] - 1);
  int endInd = mItemEmpty[3] ? -1 : (mItemInt[3] - 1);
  mNavHelper->CountHoleAcquires(startInd, endInd, minHoles, index, numHoles, numRec);
  if (index < 0)
    mLogRpt = "The Navigator is not open; there are no acquire items";
  else
    mLogRpt.Format("With a minimum of %d holes, Navigator would acquire %d holes at %d "
      "items (%d Records)", minHoles, numHoles, index, numRec);
  SetReportedValues((double)numHoles, (double)index, (double)numRec);
  return 0;
}

// ReportNumTableItems
int CMacCmd::ReportNumTableItems(void)
{
  int index;
  ABORT_NONAV;
  index = mNavigator->GetNumberOfItems();
  mLogRpt.Format("Navigator table has %d items", index);
  SetRepValsAndVars(1, (double)index);
  return 0;
}

// SetNavRegistration
int CMacCmd::SetNavRegistration(void)
{
  ABORT_NONAV;
  if (mNavigator->SetCurrentRegistration(mItemInt[1]))
    ABORT_LINE("New registration number is out of range or used for imported items "
    "in:\n\n");
  mNavigator->SetChanged(true);
  return 0;
}

// SaveNavigator
int CMacCmd::SaveNavigator(void)
{
  ABORT_NONAV;
  if (!mItemEmpty[1]) {
    SubstituteLineStripItems(mStrLine, 1, mStrCopy);
    mWinApp->mNavigator->SetCurrentNavFile(mStrCopy);
  }
  mNavigator->DoSave(false);
  return 0;
}

// ReportIfNavOpen
int CMacCmd::ReportIfNavOpen(void)
{
  int index;

  index = 0;
  mLogRpt = "Navigator is NOT open";
  if (mWinApp->mNavigator) {
    index = 1;
    mLogRpt = "Navigator IS open";
    if (!mWinApp->mNavigator->GetCurrentNavFile().IsEmpty()) {
      mLogRpt += "; file is defined";
      index = 2;
    }
  }
  SetRepValsAndVars(1, (double)index);
  return 0;
}

// ReadNavFile, MergeNavFile
int CMacCmd::ReadNavFile(void)
{
  BOOL truth;
  CFileStatus status;

  truth = CMD_IS(MERGENAVFILE);
  if (truth) {
    ABORT_NONAV;
  } else  {
    mWinApp->mMenuTargets.OpenNavigatorIfClosed();
  }
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  if (!CFile::GetStatus((LPCTSTR)mStrCopy, status))
    ABORT_LINE("The file " + mStrCopy + " does not exist in line:\n\n");
  if (mWinApp->mNavigator->LoadNavFile(false, CMD_IS(MERGENAVFILE), &mStrCopy))
    ABORT_LINE("Script stopped due to error processing Navigator file for line:\n\n");
  return 0;

}

// CloseNavigator
int CMacCmd::CloseNavigator(void)
{
  ABORT_NONAV;
  mNavigator->DoClose();
  return 0;
}

// OpenNavigator
int CMacCmd::OpenNavigator(void)
{
  if (!mItemEmpty[1] && mWinApp->mNavigator)
    ABORT_LINE("The Navigator must not be open for line:\n\n");
  mWinApp->mMenuTargets.OpenNavigatorIfClosed();
  if (!mItemEmpty[1]) {
    SubstituteLineStripItems(mStrLine, 1, mStrCopy);
    mWinApp->mNavigator->SetCurrentNavFile(mStrCopy);
  }
  return 0;
}

// ChangeItemRegistration, ChangeItemColor, ChangeItemDraw, ChangeItemLabel, 
// ChangeItemNote, ChangeItemGroupID
int CMacCmd::ChangeItemRegistration(void)
{
  CString report;
  int index, index2;
  CMapDrawItem *navItem;
  ABORT_NONAV;
  index = mItemInt[1];
  index2 = mItemInt[2];
  navItem = mNavigator->GetOtherNavItem(index - 1);
  report.Format("The Navigator item index, %d, is out of range in:\n\n", index);
  if (!navItem)
    ABORT_LINE(report);
  if (CMD_IS(CHANGEITEMREGISTRATION)) {
    report.Format("The Navigator item with index %d is a registration point in:\n\n",
      index);
    if (navItem->mRegPoint)
      ABORT_LINE(report);
    if (mNavigator->ChangeItemRegistration(index - 1, index2, report))
      ABORT_LINE(report + " in line:\n\n");
  } else {
    if (CMD_IS(CHANGEITEMCOLOR)) {
      report.Format("The Navigator item color must be between 0 and %d in:\n\n",
        NUM_ITEM_COLORS - 1);
      if (index2 < 0 || index2 >= NUM_ITEM_COLORS)
        ABORT_LINE(report);
      navItem->mColor = index2;
    } else if (CMD_IS(CHANGEITEMGROUPID)) {
      if (index2 < 0)
        ABORT_LINE("The group ID cannot be negative in line:\n\n");
      navItem->mGroupID = index2;
      if (mNavigator->m_bCollapseGroups) {
        mNavigator->MakeListMappings();
        if (!mSuspendNavRedraw)
          mNavigator->FillListBox();
      }
    } else if (CMD_IS(CHANGEITEMDRAW)) {
      if (mItemEmpty[2])
        navItem->mDraw = !navItem->mDraw;
      else
        navItem->mDraw = index2;
    } else if (CMD_IS(CHANGEITEMNOTE)) {
      SubstituteVariables(&mStrLine, 1, mStrLine);
      if (mItemEmpty[2])
        mStrCopy = "";
      else
        JustStripItems(mStrLine, 2, mStrCopy);
      navItem->mNote = mStrCopy;
    } else {
      SubstituteLineStripItems(mStrLine, 2, mStrCopy);
      report.Format("The Navigator label size must be no more than %d characters "
        "in:\n\n", MAX_LABEL_SIZE);
      if (mStrCopy.GetLength() > MAX_LABEL_SIZE)
        ABORT_LINE(report);
      navItem->mLabel = mStrCopy;
    }
    mNavigator->SetChanged(true);
    mNavigator->UpdateListString(index - 1);
    mNavigator->Redraw();
  }
  return 0;
}

// SetItemTargetDefocus
int CMacCmd::SetItemTargetDefocus(void)
{
  int index;
  CMapDrawItem *navItem;

  index = mItemInt[1];
  navItem = CurrentOrIndexedNavItem(index, mStrLine);
  if (!navItem)
    return 1;
  if (!navItem->mAcquire && navItem->mTSparamIndex < 0)
    ABORT_LINE("Specified Navigator item is not marked for acquisition in line:\n\n");
  if (mItemFlt[2] < -20 || mItemFlt[2] > 0)
    ABORT_LINE("Target defocus must be between 0 and -20 in line:\n\n:");
  navItem->mTargetDefocus = mItemFlt[2];
  mNavigator->SetChanged(true);
  mLogRpt.Format("For Navigator item %d, target defocus set to %.2f", index + 1,
    mItemFlt[2]);
  return 0;
}

// SetItemSeriesAngles
int CMacCmd::SetItemSeriesAngles(void)
{
  CString report;
  int index;
  CMapDrawItem *navItem;

  index = mItemInt[1];
  navItem = CurrentOrIndexedNavItem(index, mStrLine);
  if (!navItem)
    return 1;
  if (navItem->mTSparamIndex < 0)
    ABORT_LINE("The specified Navigator item is not marked for a tilt series"
      " in line:\n\n");
  if (mNavHelper->CheckTiltSeriesAngles(navItem->mTSparamIndex, mItemFlt[2], mItemFlt[3],
    mItemEmpty[4] ? EXTRA_NO_VALUE : mItemFlt[4], report))
    ABORT_LINE(report + "in line:\n\n");
  navItem->mTSstartAngle = mItemFlt[2];
  navItem->mTSendAngle = mItemFlt[3];
  mLogRpt.Format("For Navigator item %d, tilt series range set to %.1f to %.1f",
    index + 1, mItemFlt[2], mItemFlt[3]);
  if (!mItemEmpty[4]) {
    navItem->mTSbidirAngle = mItemFlt[4];
    report.Format(", start %.1f", mItemFlt[4]);
    mLogRpt += report;
  }
  mNavigator->SetChanged(true);
  return 0;
}

// SkipAcquiringNavItem
int CMacCmd::SkipAcquiringNavItem(void)
{
  ABORT_NONAV;
  if (!mNavigator->GetAcquiring())
    mWinApp->AppendToLog("SkipAcquiringNavItem has no effect except from a\r\n"
    "    pre-script when acquiring Navigator items", mLogAction);
  mNavigator->SetSkipAcquiringItem(mItemEmpty[1] || mItemInt[1] != 0);
  return 0;
}

// SkipAcquiringGroup
int CMacCmd::SkipAcquiringGroup(void)
{
  int index, index2;
  CMapDrawItem *navItem;
  ABORT_NONAV;
  if (!mNavigator->GetAcquiring())
    ABORT_NOLINE("The navigator must be acquiring to set a group ID to skip");
  if (mItemEmpty[1] || !mItemInt[1]) {
    index2 = mNavigator->GetCurrentOrAcquireItem(navItem);
    index = navItem->mGroupID;
  } else {
    index = mItemInt[1];
  }
  mNavigator->SetGroupIDtoSkip(index);
  if (!mItemEmpty[2] && mItemInt[2] && index) {
    mNavigator->SetGroupAcquireFlags(index, false);
  }
  return 0;
}

// SkipMoveInNavAcquire
int CMacCmd::SkipMoveInNavAcquire(void)
{
  ABORT_NONAV;
  if (!mNavigator->GetAcquiring())
    ABORT_NOLINE("The navigator must be acquiring to enable skipping the stage move");
  mNavigator->SetSkipStageMoveInAcquire(mItemEmpty[1] || mItemInt[1] != 0);
  return 0;
}

// SuspendNavRedraw
int CMacCmd::SuspendNavRedraw(void)
{
  ABORT_NONAV;
  mSuspendNavRedraw = mItemEmpty[1] || mItemInt[1] != 0;
  if (!mSuspendNavRedraw) {
    mNavigator->FillListBox(false, true);
    mNavigator->Redraw();
  }
  return 0;
}

// StartNavAcquireAtEnd
int CMacCmd::StartNavAcquireAtEnd(void)
{
  ABORT_NONAV;
  mStartNavAcqAtEnd = mItemEmpty[1] || mItemInt[1] != 0;
  if (mStartNavAcqAtEnd && (mWinApp->DoingTiltSeries() || mNavigator->GetAcquiring()))
    ABORT_NOLINE(CString("You cannot use StartNavAcquireAtEnd when ") +
      (mWinApp->DoingTiltSeries() ? "a tilt series is running" :
      "Navigator is already acquiring"));
  return 0;
}

// LoadScriptPackAtEnd
int CMacCmd::LoadScriptPackAtEnd(void)
{
  if (mWinApp->DoingTiltSeries() || (mNavigator && mNavigator->GetAcquiring()))
    ABORT_NOLINE(CString("You cannot use LoadScriptPackAtEnd when ") +
    (mWinApp->DoingTiltSeries() ? "a tilt series is running" :
      "Navigator is already acquiring"));
  mSaveCurrentPack = mItemInt[1] != 0;
  SubstituteLineStripItems(mStrLine, 2, mPackToLoadAtEnd);
  return 0;
}

// SuffixForExtraFile
int CMacCmd::SuffixForExtraFile(void)
{
  ABORT_NONAV;
  mNavigator->SetExtraFileSuffixes(&mStrItems[1],
    B3DMIN(mLastNonEmptyInd, MAX_STORES - 1));
  return 0;
}

// ItemForSuperCoord
int CMacCmd::ItemForSuperCoord(void)
{
  ABORT_NONAV;
  mNavigator->SetSuperCoordIndex(mItemInt[1] - 1);
  return 0;
}

// UpdateItemZ, UpdateGroupZ
int CMacCmd::UpdateItemZ(void)
{
  int index, index2;
  CMapDrawItem *navItem;
  ABORT_NONAV;
  index2 = CMD_IS(UPDATEGROUPZ) ? 1 : 0;
  if (!mItemEmpty[1] && mItemInt[1]) {
    if (index2) {
      if (!mNavigator->GetAcquiring())
        ABORT_LINE("The Navigator needs to be acquiring for updating the next group Z in "
          "line:\n\n");
      if (!mNavigator->FindNextAcquireItem(index))
        ABORT_LINE("There is no next Navigator item to be acquired for line:\n\n");
    } else {
      index = mItemInt[1] - 1;
      if (index < 0 || index >= mNavigator->GetNumberOfItems())
        ABORT_LINE("Index of Navigator item is out of range in line:\n\n");
    }
  } else
    index = mNavigator->GetCurrentOrAcquireItem(navItem);
  if (index < 0)
    ABORT_NOLINE("There is no current Navigator item.");
  index = mNavigator->DoUpdateZ(index, index2);
  if (index == 3)
    ABORT_LINE("Current Navigator item is not in a group for statement:\n\n");
  if (index)
    ABORT_LINE("Error updating Z of Navigator item in statement:\n\n");
  mNavigator->SetChanged(true);
  return 0;
}

// ReportGroupStatus
int CMacCmd::ReportGroupStatus(void)
{
  CString report;
  int index, index2, ix0, ix1;
  CMapDrawItem *navItem;
  ABORT_NONAV;
  index = mNavigator->GetCurrentOrAcquireItem(navItem);
  if (index < 0)
    ABORT_NOLINE("There is no current Navigator item.");
  index2 = navItem->mGroupID;
  index = -1;
  ix0 = mNavigator->CountItemsInGroup(index2, report, mStrCopy, ix1);
  if (mNavigator->GetAcquiring()) {
    index = 0;
    if (index2)
      index = mNavigator->GetFirstInGroup() ? 1 : 2;
  }
  mLogRpt.Format("Group acquire status %d, group ID %d, # in group %d, %d set to acquire"
    , index, index2, ix0, ix1);
  SetRepValsAndVars(1, (double)index, (double)index2, (double)ix0,
    (double)ix1);
  return 0;
}

// ReportItemImageCoords
int CMacCmd::ReportItemImageCoords(void)
{
  CString report;
  int index, index2, sizeX, sizeY;
  float floatX, floatY;
  CMapDrawItem *navItem;

  index = mItemEmpty[1] ? 0 : mItemInt[1];
  navItem = CurrentOrIndexedNavItem(index, mStrLine);
  if (!navItem)
    return 1;
  if (ConvertBufferLetter(mStrItems[2], 0, true, index2, report))
    ABORT_LINE(report);
  if (mWinApp->mMainView->GetMapItemsForImageCoords(&mImBufs[index2], true)) {
    mWinApp->mMainView->GetItemImageCoords(&mImBufs[index2], navItem, floatX,
      floatY);
    mImBufs[index2].mImage->getSize(sizeX, sizeY);
    if (floatX >= 0 && floatY >= 0 && floatX <= (sizeX - 1) &&
      floatY <= (sizeY - 1)) {
      SetReportedValues(floatX, floatY, 1);
      mLogRpt.Format("Navigator item %d has image coordinates %.2f, %.2f on %c",
        index + 1, floatX, floatY, index2 + 65);
    } else {
      SetReportedValues(floatX, floatY, 0);
      mLogRpt.Format("Navigator item %d is outside of %c", index + 1, index2 + 65);
    }
  } else {
    SetReportedValues(0.0, 0.0, -1);
    mLogRpt.Format("Navigator item %d could not be transformed to %c", index + 1,
      index2 + 65);
  }
  return 0;
}

// NewMap
int CMacCmd::NewMap(void)
{
  int index;
  ABORT_NONAV;
  mNavigator->SetSkipBacklashType(1);
  index = 0;
  if (!mItemEmpty[1]) {
    index = mItemInt[1];
    if (mItemEmpty[2])
      ABORT_LINE("There must be text for the Navigator note after the number in:\n\n");
    SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  }
  if (mNavigator->NewMap(false, index, mItemEmpty[1] ? NULL : &mStrCopy)) {
    mCurrentIndex = mLastIndex;
    SuspendMacro();
    return 1;
  }
  SetRepValsAndVars(1, (double)mNavigator->GetNumberOfItems());
  return 0;
}

// MakeAnchorMap
int CMacCmd::MakeAnchorMap(void)
{
  ABORT_NONAV;
  if (mNavigator->DoMakeDualMap()) {
    AbortMacro();
    return 1;
  }
  mMakingDualMap = true;
  return 0;
}

// ShiftItemsByAlignment
int CMacCmd::ShiftItemsByAlignment(void)
{
  ABORT_NONAV;
  if (mNavigator->ShiftItemsByAlign()) {
    AbortMacro();
    return 1;
  }
  mNavigator->SetChanged(true);
  return 0;
}

// ShiftItemsByCurrentDiff
int CMacCmd::ShiftItemsByCurrentDiff(void)
{
  double delISX, delISY, stageX, stageY, stageZ, specDist;
  int index, index2;
  float shiftX, shiftY;
  CMapDrawItem *navItem;
  ABORT_NONAV;
  index = mNavigator->GetCurrentOrAcquireItem(navItem);
  if (index < 0)
    ABORT_NOLINE("There is no current Navigator item.");
  mScope->GetStagePosition(stageX, stageY, stageZ);
  shiftX = (float)stageX;
  shiftY = (float)stageY;
  mScope->GetImageShift(delISX, delISY);
  index2 = mScope->GetLowDoseArea();
  if (mWinApp->LowDoseMode() && (index2 == VIEW_CONSET || index2 == SEARCH_AREA)) {
    mWinApp->mLowDoseDlg.GetNetViewShift(stageX, stageY, index2);
    delISX -= stageX;
    delISY -= stageY;
  }
  mNavigator->ConvertIStoStageIncrement(mScope->FastMagIndex(),
      mCurrentCam, delISX, delISY, (float)mScope->FastTiltAngle(),
      shiftX, shiftY);
  shiftX -= navItem->mStageX;
  shiftY -= navItem->mStageY;
  specDist = sqrt(shiftX * shiftX + shiftY * shiftY);
  if (specDist <= mItemDbl[1]) {
    mNavigator->ShiftItemsAtRegistration(shiftX, shiftY,
      navItem->mRegistration);
    PrintfToLog("Items at registration %d shifted by %.2f, %.2f",
      navItem->mRegistration, shiftX, shiftY);
    mNavigator->SetChanged(true);
  } else {
    PrintfToLog("Current stage position is too far from item position (%.2f microns);"
      "nothing was shifted", specDist);
  }
  return 0;
}

// ShiftItemsByMicrons
int CMacCmd::ShiftItemsByMicrons(void)
{
  CString report;
  int index, index2;
  ABORT_NONAV;
  if (fabs(mItemDbl[1]) > 100. || fabs(mItemDbl[2]) > 100.)
    ABORT_LINE("You cannot shift items by more than 100 microns in:\n\n");
  index = mNavigator->GetCurrentRegistration();
  if (!mItemEmpty[3]) {
    index = mItemInt[3];
    report.Format("Registration number %d is out of range in:\n\n", index);
    if (index <= 0 || index > MAX_CURRENT_REG)
      ABORT_LINE(report);
  }
  index2 = mNavigator->ShiftItemsAtRegistration(mItemFlt[1], mItemFlt[2],
    index);
  mLogRpt.Format("%d items at registration %d were shifted by %.2f, %.2f", index2,
    index, mItemDbl[1], mItemDbl[2]);
  mNavigator->SetChanged(true);
  return 0;
}

// AlignAndTransformItems
int CMacCmd::AlignAndTransformItems(void)
{
  CString report;
  int index, index2, ix0;
  float backlashX;

  index2 = mNavHelper->BufferForRotAlign(index);
  if (index != mNavigator->GetCurrentRegistration()) {
    report.Format("Image in buffer %c is at registration %d, not the current Navigator"
      " registration, %d, for line:\n\n", 'A' + index2, index,
      mNavigator->GetCurrentRegistration());
    ABORT_LINE(report);
  }
  ix0 = CNavRotAlignDlg::CheckAndSetupReference();
  if (ix0 < 0)
    ABORT_LINE("Cannot find a Navigator item corresponding to the image in the Read"
      " buffer for line:\n\n");
  if (ix0 == index)
    ABORT_LINE("The image in the Read buffer is at the current Navigator"
      " registration for line:\n\n");
  if (fabs(mItemDbl[2]) < 0.2 || fabs(mItemDbl[2]) > 50.)
    ABORT_LINE("Angular range to search must be between 0.2 and 50 degrees for "
      "line:\n\n");
  CNavRotAlignDlg::PrepareToUseImage();
  if (CNavRotAlignDlg::AlignToMap(mItemFlt[1], mItemFlt[2], backlashX)) {
    AbortMacro();
    return 1;
  }
  if (!mItemEmpty[3] && fabs(backlashX - mItemDbl[1]) > mItemDbl[3]) {
    mLogRpt.Format("The rotation found by alignment, %.1f degrees, is farther from\r\n"
      " the center angle than the specified limit, nothing was transformed", backlashX);
  } else {
    index = CNavRotAlignDlg::TransformItemsFromAlignment();
    mLogRpt.Format("%d items transformed after alignment with rotation of %.1f degrees",
      index, backlashX);
    mNavigator->SetChanged(true);
  }
  SetReportedValues(backlashX);
  return 0;
}

// ForceCenterRealign
int CMacCmd::ForceCenterRealign(void)
{
  ABORT_NONAV;
  mNavHelper->ForceCenterRealign();
  return 0;
}

// SkipPiecesOutsideItem
int CMacCmd::SkipPiecesOutsideItem(void)
{
  if (!mWinApp->Montaging())
    ABORT_LINE("Montaging must be on already to use this command:\n\n");
  if (mItemInt[1] >= 0)
    mMontP->insideNavItem = mItemInt[1] - 1;
  mMontP->skipOutsidePoly = mItemInt[1] >= 0;
  return 0;
}

// FindHoles
int CMacCmd::FindHoles(void)
{
  CString report;
  EMimageBuffer *imBuf;
  int index;
  ABORT_NONAV;
  imBuf = mWinApp->mMainView->GetActiveImBuf();
  if (!mItemEmpty[1]) {
    if (ConvertBufferLetter(mStrItems[1], -1, true, index, report))
      ABORT_LINE(report);
    imBuf = &mImBufs[index];
  }
  if (mNavHelper->mHoleFinderDlg->DoFindHoles(imBuf)) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// ReportHoleFinderParams
int CMacCmd::ReportHoleFinderParams(void)
{
  HoleFinderParams *hfp;
  mNavHelper->mHoleFinderDlg->SyncToMasterParams();
  hfp = mNavHelper->GetHoleFinderParams();
  mLogRpt.Format("Hole size is %.2f and spacing is %.2f um", hfp->diameter, hfp->spacing);
  if (hfp->hexagonalArray)
    mLogRpt += " for hexagonal array";
  mStrCopy.Format("  max error %.3f um", hfp->maxError);
  SetRepValsAndVars(1, hfp->diameter, hfp->spacing, hfp->hexagonalArray ? 1 : 0,
    hfp->maxError);
  return 0;
}

// SetHoleFinderParams
int CMacCmd::SetHoleFinderParams(void)
{
  HoleFinderParams *hfp;
  Variable *var[2] = {NULL, NULL};
  int ind, index;
  if (mItemFlt[1] < 0.05f || mItemFlt[2] < 0.15f || mItemFlt[2] - 0.1 < mItemFlt[1])
    ABORT_LINE("Size must be at least 0.1 and spacing must be at least 0.15 and 0.1 more"
      " than size for line:\n\n");
  for (ind = 0; ind < 2; ind++) {
    if (!mItemEmpty[5 + ind] && (ind || mStrItems[5 + ind] != "NONE")) {
      var[ind] = LookupVariable(mStrItems[ind + 5], index);
      if (!var[ind])
        ABORT_LINE("The variable " + mStrItems[5 + ind] + " is not defined in line:\n\n");
    }
  }
  mNavHelper->mHoleFinderDlg->SyncToMasterParams();
  hfp = mNavHelper->GetHoleFinderParams();
  hfp->hexagonalArray = mItemInt[3] != 0;
  if (!mItemInt[3]) {
    hfp->diameter = mItemFlt[1];
    hfp->spacing = mItemFlt[2];
  } else {
    hfp->hexDiameter = mItemFlt[1];
    hfp->hexSpacing = mItemFlt[2];
  }
  if (!mItemEmpty[4] && mItemFlt[4] > 0.)
    hfp->maxError = mItemFlt[4];
  if (var[0])
    FillVectorFromArrayVariable(&hfp->sigmas, NULL, var[0]);
  if (var[1])
    FillVectorFromArrayVariable(&hfp->thresholds, NULL, var[1]);
  if (mNavHelper->mHoleFinderDlg->IsOpen())
    mNavHelper->mHoleFinderDlg->UpdateSettings();
  return 0;
}

// ReportLastHoleVectors, UseHoleVectorsForMulti
int CMacCmd::ReportLastHoleVectors(void)
{
  int numDir, index = mItemInt[1];
  double xVecs[3], yVecs[3];
  bool settingMulti = CMD_IS(USEHOLEVECTORSFORMULTI);
  MultiShotParams *msParams;
  if (index > 0 && (index >= MAX_MAGS || !mMagTab[index].mag))
    ABORT_LINE("The magnification index is out of range in:\n\n");
  if (settingMulti) {
    mNavHelper->UpdateMultishotIfOpen();
    msParams = mNavHelper->GetMultiShotParams();
    if (index < 0) {
      index = msParams->holeMagIndex;
      if (!index)
        ABORT_LINE("There is no existing hole magnification index to use for:\n\n");
    }
    if (!index)
      index = mLdParam[RECORD_CONSET].magIndex;
    if (!index)
      ABORT_LINE("There is no Record magnification index to use for:\n\n");
  }
  numDir = mNavHelper->mHoleFinderDlg->ConvertHoleToISVectors(index, settingMulti, xVecs,
    yVecs, mStrCopy);
  if (!numDir)
    ABORT_LINE(mStrCopy + " for line:\n\n");
  if (!settingMulti) {
    if (numDir > 2)
      SetRepValsAndVars(2, xVecs[0], yVecs[0], xVecs[1], yVecs[1], xVecs[2], yVecs[2]);
    else
      SetRepValsAndVars(2, xVecs[0], yVecs[0], xVecs[1], yVecs[1]);
  }
  if (numDir > 2)
    mLogRpt.Format("Hole %s vectors are %.4g, %.4g  %.4g, %.4g and %.4g, %.4g", 
      B3DCHOICE(index < 0, "image", index ? "IS" : "stage"), xVecs[0], yVecs[0], xVecs[1],
      yVecs[1], xVecs[2], yVecs[2]);
  else
    mLogRpt.Format("Hole %s vector  s are %.4g, %.4g and %.4g, %.4g", B3DCHOICE(index < 0,
      "image", index ? "IS" : "stage"), xVecs[0], yVecs[0], xVecs[1], yVecs[1]);
  return 0;
}

// MakeNavPointsAtHoles
int CMacCmd::MakeNavPointsAtHoles(void)
{
  int index;
  ABORT_NONAV;
  index = mItemEmpty[1] || mItemInt[1] < 0 ? - 1 : mItemInt[1];
  if (index > 2)
    ABORT_LINE("The layout type must be less than 3 for line:\n\n");
  index = mNavHelper->mHoleFinderDlg->DoMakeNavPoints(index,
    (float)((mItemEmpty[2] || mItemDbl[2] < -900000) ? EXTRA_NO_VALUE : mItemDbl[2]),
    (float)((mItemEmpty[3] || mItemDbl[3] < -900000) ? EXTRA_NO_VALUE : mItemDbl[3]),
    (float)((mItemEmpty[4] || mItemDbl[4] < 0.) ? - 1. : mItemDbl[4]) / 100.f,
    (float)((mItemEmpty[5] || mItemDbl[5] < 0.) ? - 1. : mItemDbl[5]) / 100.f,
    (mItemEmpty[6] || mItemFlt[6] < 0.) ? -1.f : mItemFlt[6]);
  if (index < 0) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Hole finder made %d Navigator points", index);
  return 0;
}

// ClearHoleFinder
int CMacCmd::ClearHoleFinder(void)
{
  mNavHelper->mHoleFinderDlg->OnButClearData();
  return 0;
}

// CombineHolesToMulti
int CMacCmd::CombineHolesToMulti(void)
{
  int index;
  ABORT_NONAV;
  B3DCLAMP(mItemInt[1], 0, 2);
  index = mNavHelper->mCombineHoles->CombineItems(mItemInt[1],
    mItemEmpty[2] ? mNavHelper->GetMHCturnOffOutsidePoly() : mItemInt[2] != 0);
  if (index)
    ABORT_NOLINE("Error trying to combine hole for multiple Records:\n" +
      CString(mNavHelper->mCombineHoles->GetErrorMessage(index)));
  return 0;
}

// UndoHoleCombining
int CMacCmd::UndoHoleCombining(void)
{
  ABORT_NONAV;
  if (!mWinApp->mNavHelper->mCombineHoles->OKtoUndoCombine())
    ABORT_NOLINE("It is no longer possible to undo the last hole combination");
  mWinApp->mNavHelper->mCombineHoles->UndoCombination();
  return 0;
}

// AutoContourGridSquares
int CMacCmd::AutoContourGridSquares(void)
{
  ABORT_NONAV;
  int index;
  mNavHelper->mAutoContouringDlg->SyncToMasterParams();
  AutoContourParams *param = mNavHelper->GetAutocontourParams();
  float target = param->usePixSize ? param->targetPixSizeUm :
    param->targetSizePixels;
  float minSize = (mItemEmpty[5] || mItemFlt[5] < 0) ? param->minSize : mItemFlt[5];
  float maxSize = (mItemEmpty[6] || mItemFlt[6] < 0) ? param->maxSize : mItemFlt[6];
  float relThresh = param->useAbsThresh ? 0.f : param->relThreshold;
  float absThresh = param->useAbsThresh ? param->absThreshold : 0.f;
  if (!mItemEmpty[2] && mItemFlt[2] >= 0)
    target = mItemFlt[2];
  if (!mItemEmpty[3] && mItemFlt[3] >= 0)
    relThresh = mItemFlt[3];
  if (!mItemEmpty[4] && mItemFlt[4] >= 0)
    absThresh = mItemFlt[4];
  if (ConvertBufferLetter(mStrItems[1], 0, true, index, mStrCopy))
    ABORT_LINE(mStrCopy);
  mNavHelper->mAutoContouringDlg->AutoContourImage(&mImBufs[index], target, minSize, 
    maxSize, relThresh, absThresh);
  mAutoContouring = true;
  return 0;
}

// MakePolygonsAtSquares
int CMacCmd::MakePolygonsAtSquares(void)
{
  if (mNavHelper->mAutoContouringDlg->ExternalCreatePolys(mItemEmpty[1] ? -1.f : 
    mItemFlt[1], mItemEmpty[2] ? -1.f : mItemFlt[2], mItemEmpty[3] ? -1.f : mItemFlt[3],
    mItemEmpty[4] ? -1.f : mItemFlt[4], mItemEmpty[5] ? -1.f : mItemFlt[5], 
    mItemEmpty[6] ? -1.f : mItemFlt[6], mStrCopy))
    ABORT_NOLINE("Failed to convert grid square contours to polygons: " + mStrCopy);
  mLogRpt = "Converted grid square contours to Navigator polygons";
  return 0;
}

// SetAutocontourGroups
int CMacCmd::SetAutocontourGroups(void)
{
  int showGroup[MAX_AUTOCONT_GROUPS];
  int ind, numShow = 0;
  if (!mItemInt[1] || mItemInt[1] > MAX_AUTOCONT_GROUPS)
    ABORT_LINE("Number of groups is out of range in line:\n\n");
  for (ind = 3; ind < 3 + MAX_AUTOCONT_GROUPS; ind++) {
    if (mItemEmpty[ind])
      break;
    showGroup[numShow++] = mItemInt[ind];
  }
  mNavHelper->mAutoContouringDlg->ExternalSetGroups(mItemInt[1], mItemInt[2], showGroup,
    numShow);
  return 0;
}

// ReportAutocontourStats
int CMacCmd::ReportAutocontourStats(void)
{
  float minMean, maxMean, median;
  if (mNavHelper->mAutoContouringDlg->GetSquareStats(minMean, maxMean, median))
    ABORT_LINE("There are no contours to give statistics on for line:\n\n");
  mLogRpt.Format("Grid square means: min %.1f  max %.1f  median %.1f",
    minMean, maxMean, median);
  SetRepValsAndVars(1, minMean, maxMean, median);
  return 0;
}

// SetHelperParams
int CMacCmd::SetHelperParams(void)
{
  mNavHelper->SetTestParams(&mItemDbl[1]);
  return 0;
}

// SetMontageParams
int CMacCmd::SetMontageParams(void)
{
  CString report;
  int index;

  if (!mWinApp->Montaging())
    ABORT_LINE("Montaging must be on already to use this command:\n\n");
  if (mWinApp->mStoreMRC && mWinApp->mStoreMRC->getDepth() > 0 &&
    ((mItemInt[2] > 0) || (mItemInt[3] > 0) ||
    (mItemInt[4] > 0) || (mItemInt[5] > 0)))
      ABORT_LINE("After writing to the file, you cannot change frame size or overlaps "
        " in line:\n\n");
  if (mItemInt[1] >= 0)
    mMontP->moveStage = mItemInt[1] > 0;
  if (!mItemEmpty[4] && mItemInt[4] > 0) {
    if (mItemInt[4] < mMontP->xOverlap * 2)
      ABORT_LINE("The X frame size is less than twice the overlap in statement:\n\n");
    mMontP->xFrame = mItemInt[4];
  }
  if (!mItemEmpty[5] && mItemInt[5] > 0) {
    if (mItemInt[5] < mMontP->yOverlap * 2)
      ABORT_LINE("The Y frame size is less than twice the overlap in statement:\n\n");
    mMontP->yFrame = mItemInt[5];
  }
  if (!mItemEmpty[2] && mItemInt[2] > 0) {
    if (mItemInt[2] > mMontP->xFrame / 2)
      ABORT_LINE("X overlap is more than half the frame size in statement:\n\n");
    if (mItemInt[2] < 16)
      ABORT_LINE("The X overlap must be at least 16 in line:\n\n");
    mMontP->xOverlap = mItemInt[2];
  }
  if (!mItemEmpty[3] && mItemInt[3] > 0) {
    if (mItemInt[3] > mMontP->yFrame / 2)
      ABORT_LINE("Y overlap is more than half the frame size in statement:\n\n");
    if (mItemInt[2] < 16)
      ABORT_LINE("The Y overlap must be at least 16 in line:\n\n");
    mMontP->yOverlap = mItemInt[3];
  }
  if (!mItemEmpty[6] && mItemInt[6] >= 0)
    mMontP->skipCorrelations = mItemInt[6] != 0;
  if (!mItemEmpty[7] && mItemDbl[7] >= 0.5) {
    if (CheckCameraBinning(mItemDbl[7], index, report))
      ABORT_LINE(report);
    mMontP->binning = index;
  }
  mWinApp->mMontageController->SetNeedBoxSetup(true);
  mWinApp->mMontageWindow.UpdateSettings();
  return 0;
}

// ParamSetToUseForMontage
int CMacCmd::ParamSetToUseForMontage(void)
{
  int index = B3DABS(mItemInt[1]);
  if (index > 1 && mWinApp->LowDoseMode() && mWinApp->Montaging())
    ABORT_LINE("The parameter set to use in Low Dose mode should be selected before a "
      "montage is opened")
    if (index == 1)
      mMontP->useMontMapParams = mItemInt[1] > 0;
  if (index == 2) {
    mMontP->useViewInLowDose = mItemInt[1] > 0;
    if (mMontP->useViewInLowDose)
      mMontP->useSearchInLowDose = false;
  }
  if (index == 3) {
    mMontP->useSearchInLowDose = mItemInt[1] > 0;
    if (mMontP->useSearchInLowDose)
      mMontP->useViewInLowDose = false;
  }
  mWinApp->mMontageController->SetNeedBoxSetup(true);
  return 0;
}

// ManualFilmExposure
int CMacCmd::ManualFilmExposure(void)
{
  double delX;

  delX = mItemDbl[1];
  mScope->SetManualExposure(delX);
  return 0;
}

// ExposeFilm, SpecialExposeFilm
int CMacCmd::ExposeFilm(void)
{
  double delX, delY;
  int index;

  delX = 0.;
  delY = 0.;
  index = 0;
  if (CMD_IS(SPECIALEXPOSEFILM)) {
    delX = mItemDbl[1];
    if (!mItemEmpty[2]) delY = mItemDbl[2];
    if (delX < 0. || delY < 0. || mItemEmpty[1])
      ABORT_LINE("There must be one or two non-negative values in statement:\n\n");
    if (!mItemEmpty[3])
      index = mItemInt[3];
  }
  if (!mScope->TakeFilmExposure(CMD_IS(SPECIALEXPOSEFILM), delX, delY, index != 0)) {
    mCurrentIndex = mLastIndex;
    SuspendMacro();
    return 1;
  }
  mExposedFilm = true;
  return 0;
}

// GoToLowDoseArea
int CMacCmd::GoToLowDoseArea(void)
{
  int index;

  if (CheckAndConvertLDAreaLetter(mStrItems[1], 1, index, mStrLine))
    return 1;
  mScope->GotoLowDoseArea(index);
  return 0;
}

// SetLDContinuousUpdate
int CMacCmd::SetLDContinuousUpdate(void)
{
  if (mWinApp->mTSController->DoingTiltSeries())
    ABORT_NOLINE("You cannot use SetLDContinuousUpdate during a tilt series");
  mWinApp->mLowDoseDlg.SetContinuousUpdate(mItemInt[1] != 0);
  return 0;
}

// SetLowDoseMode
int CMacCmd::SetLowDoseMode(void)
{
  int index;

  index = mWinApp->LowDoseMode() ? 1 : 0;
  if (index != (mItemInt[1] ? 1 : 0))
    mWinApp->mLowDoseDlg.SetLowDoseMode(mItemInt[1] != 0);
  SetRepValsAndVars(2, (double)index);
  return 0;
}

// SetAxisPosition, ReportAxisPosition
int CMacCmd::SetAxisPosition(void)
{
  double delX, delY;
  int index, index2, ix0, iy0;

  if (CheckAndConvertLDAreaLetter(mStrItems[1], 1, index, mStrLine))
    return 1;
  if ((index + 1) / 2 != 1)
    ABORT_LINE("This command must be followed by F or T:\n\n");
  if (CMD_IS(REPORTAXISPOSITION)) {
    delX = mWinApp->mLowDoseDlg.ConvertOneIStoAxis(mLdParam[index].magIndex,
      mLdParam[index].ISX, mLdParam[index].ISY);
    ix0 = ((mConSets[index].right + mConSets[index].left) / 2 - mCamParams->sizeX / 2)
      / BinDivisorI(mCamParams);
    iy0 = ((mConSets[index].bottom + mConSets[index].top) / 2 - mCamParams->sizeY / 2)
      / BinDivisorI(mCamParams);
    index2 = mWinApp->mLowDoseDlg.m_bRotateAxis ? mWinApp->mLowDoseDlg.m_iAxisAngle : 0;
    mLogRpt.Format("%s axis position %.2f microns, %d degrees; camera offset %d, %d "
      "unbinned pixels", mModeNames[index], delX, index2, ix0, iy0);
    SetRepValsAndVars(2, delX, (double)index2, (double)ix0, (double)iy0);

  } else {
    index2 = 0;
    if (fabs(mItemDbl[2]) > 19.)
      ABORT_LINE("The axis distance is too large in:\n\n");
    if (!mItemEmpty[3])
      index2 = B3DNINT(UtilGoodAngle(mItemDbl[3]));
    ix0 = (mScope->GetLowDoseArea() + 1) / 2;
    if (ix0 == 1)
      mScope->GetLDCenteredShift(delX, delY);
    mWinApp->mLowDoseDlg.NewAxisPosition(index, mItemDbl[2], index2, !mItemEmpty[3]);
    if (ix0 == 1)
      mScope->SetLDCenteredShift(delX, delY);
  }
  return 0;
}

// ReportLowDose
int CMacCmd::ReportLowDose(void)
{
  int index, index2;
  char *modeLets = "VFTRS";
  index = mWinApp->LowDoseMode() ? 1 : 0;
  index2 = mScope->GetLowDoseArea();
  mLogRpt.Format("Low Dose is %s%s%c", index ? "ON" : "OFF",
    index && index2 >= 0 ? " in " : "",
    index && index2 >= 0 ? modeLets[index2] : ' ');
  SetRepValsAndVars(1, (double)index, (double)index2);
  return 0;
}

// CurrentSettingsToLDArea
int CMacCmd::CurrentSettingsToLDArea(void)
{
  int index;

  if (CheckAndConvertLDAreaLetter(mStrItems[1], -1, index, mStrLine))
    return 1;
  mWinApp->InitializeOneLDParam(mLdParam[index]);
  mWinApp->mLowDoseDlg.SetLowDoseMode(true);
  mScope->GotoLowDoseArea(index);
  return 0;
}

// UpdateLowDoseParams
int CMacCmd::UpdateLowDoseParams(void)
{
  int index;

  if (mWinApp->mTSController->DoingTiltSeries())
    ABORT_NOLINE("You cannot use ChangeLowDoseParams during a tilt series");
  if (CheckAndConvertLDAreaLetter(mStrItems[1], 0, index, mStrLine))
    return 1;
  if (!mKeepOrRestoreArea[index]) {
    mLDParamsSaved.push_back(mLdParam[index]);
    mLDareasSaved.push_back(index);
    mKeepOrRestoreArea[index] = (mItemEmpty[2] || !mItemInt[2]) ? 1 : -1;
    UpdateLDAreaIfSaved();
  }
  return 0;
}

// RestoreLowDoseParamsCmd
int CMacCmd::RestoreLowDoseParamsCmd(void)
{
  int index;

  if (CheckAndConvertLDAreaLetter(mStrItems[1], 0, index, mStrLine))
    return 1;
  RestoreLowDoseParams(index);
  return 0;
}

// SetLDAddedBeamButton
int CMacCmd::SetLDAddedBeamButton(void)
{
  BOOL truth;

  truth = mItemEmpty[1] || mItemInt[1];
  if (!mWinApp->LowDoseMode() || mScope->GetLowDoseArea() < 0)
    ABORT_LINE("You must be in Low Dose mode and in a defined area for line:\n\n");
  if (mLDSetAddedBeamRestore < 0) {
    mLDSetAddedBeamRestore = mWinApp->mLowDoseDlg.m_bSetBeamShift ? 1 : 0;
    mNumStatesToRestore++;
  }
  mWinApp->mLowDoseDlg.SetBeamShiftButton(truth);
  if ((truth ? 1 : 0) == mLDSetAddedBeamRestore) {
    mLDSetAddedBeamRestore = -1;
    mNumStatesToRestore--;
  }
  return 0;
}

// CopyLowDoseArea
int CMacCmd::CopyLowDoseArea(void)
{
  int from, area;
  if (!mWinApp->LowDoseMode())
    ABORT_LINE("You must be in low dose mode for line:\n\n");
  if (CheckAndConvertLDAreaLetter(mStrItems[1], 1, from, mStrLine) ||
    CheckAndConvertLDAreaLetter(mStrItems[2], 1, area, mStrLine))
    return 1;
  if (from == area)
    ABORT_LINE("You must specify two different areas in line:\n\n");
  if (mScope->GetLowDoseArea() == area)
    ABORT_LINE("You cannot copy TO the current low dose area in:\n\n");
  mWinApp->mLowDoseDlg.DoCopyArea(from, area);
  mLogRpt.Format("Copied low dose area %s to %s", mStrItems[1], mStrItems[2]);
  return 0;
}

// ShowMessageOnScope
int CMacCmd::ShowMessageOnScope(void)
{
  CString report;
  int index, iy1;

  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  if (!FEIscope)
    ABORT_LINE("This command can be run only on an FEI scope");
  if (mScope->GetPluginVersion() < FEI_PLUGIN_MESSAGE_BOX) {
    report.Format("You need to upgrade the FEI plugin and/or server\n"
      "to at least version %d get a message box in:\n\n", FEI_PLUGIN_MESSAGE_BOX);
    ABORT_LINE(report);
  }
  mScope->SetMessageBoxArgs(mBoxOnScopeType, mStrCopy, mBoxOnScopeText);
  iy1 = LONG_OP_MESSAGE_BOX;
  index = mScope->StartLongOperation(&iy1, &mBoxOnScopeInterval, 1);
  if (index > 0)
    ABORT_LINE("The thread is already busy for a long operation so cannot run:\n\n");
  if (!index) {
    mStartedLongOp = true;
    mShowedScopeBox = true;
  } else {
    SetReportedValues(0., -1);
  }
  return 0;
}

// SetupScopeMessage
int CMacCmd::SetupScopeMessage(void)
{
  mBoxOnScopeType = mItemInt[1];
  if (!mItemEmpty[2])
    mBoxOnScopeInterval = mItemFlt[2];
  if (!mItemEmpty[3]) {
    SubstituteLineStripItems(mStrLine, 3, mBoxOnScopeText);
  }
  return 0;
}

// UpdateHWDarkRef
int CMacCmd::UpdateHWDarkRef(void)
{
  int index;

  index = mCamera->UpdateK2HWDarkRef(mItemFlt[1]);
  if (index == 1)
    ABORT_LINE("The thread is already busy for this operation:\n\n")
  mStartedLongOp = !index;
  return 0;
}

// RunGatanScript
int CMacCmd::RunGatanScript(void)
{
  int index;
  Variable *var = LookupVariable(mStrItems[1], index);
  if (!var)
    ABORT_LINE("The variable " + mStrItems[1] + " is not defined in line:\n\n");
  if (mCamera->RunScriptOnThread(var->value, mItemEmpty[2] ? 0 : mItemInt[2]))
    ABORT_LINE("The current camera must be a Gatan camera for line:\n\n");
  mRanGatanScript = true;
  return 0;
}

// LongOperation
int CMacCmd::LongOperation(void)
{
  CString report;
  int index, index2, ix1, iy1;
  float backlashX;

  ix1 = 0;
  iy1 = 1;
  int used[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int operations[MAX_LONG_OPERATIONS + 1];
  float intervals[MAX_LONG_OPERATIONS + 1];
  for (index = 1; index < MAX_MACRO_TOKENS && !mItemEmpty[index]; index++) {
    for (index2 = 0; index2 < MAX_LONG_OPERATIONS; index2++) {
      if (!mStrItems[index].Left(2).CompareNoCase(CString(mLongKeys[index2]))) {
        if (mLongHasTime[index2] && mItemDbl[index + 1] < -1.5) {
          backlashX = mItemFlt[index + 1];
          mScope->StartLongOperation(&index2, &backlashX, 1);
          SetOneReportedValue(backlashX, iy1++);
          report.Format("Time since last long operation %s is %.2f hours",
            (LPCTSTR)mStrItems[index], backlashX);
          mWinApp->AppendToLog(report, mLogAction);
          index++;
          break;
        }
        if (used[index2])
          ABORT_LINE("The same operation is specified twice in:\n\n");
        used[index2]++;
        operations[ix1] = index2;
        if (index2 == LONG_OP_UNLOAD_CART && JEOLscope)
          ABORT_LINE("For JEOL, use UnloadCartridge to unload a cartridge instead"
            " of:\n\n");
        if (index2 == LONG_OP_HW_DARK_REF &&
          !mCamera->CanDoK2HardwareDarkRef(mCamParams, report))
          ABORT_LINE(report + " in line:\n\n");
        if (mLongHasTime[index2]) {
          if (index == MAX_MACRO_TOKENS - 1 || mItemEmpty[index + 1])
            ABORT_LINE("The last operation must be followed by an interval in hours "
              "in:\n\n");
          report = mStrItems[index + 1];
          if (report.MakeLower() != mStrItems[index + 1]) {
            report.Format("%s must be followed by an interval in hours in:\n\n",
              (LPCTSTR)mStrItems[index]);
            ABORT_LINE(report);
          }
          index++;
          intervals[ix1++] = mItemFlt[index];
        }
        else
          intervals[ix1++] = 0.;
        break;
      }
    }
    if (index2 == MAX_LONG_OPERATIONS) {
      report.Format("%s is not an allowed entry for a long operation in:\n\n",
        (LPCTSTR)mStrItems[index]);
      ABORT_LINE(report);
    }
  }
  index = mScope->StartLongOperation(operations, intervals, ix1);
  if (index > 0)
    ABORT_LINE(index == 1 ? "The thread is already busy for a long operation in:\n\n" :
      "A long scope operation can be done only on an FEI scope for:\n\n");
  mStartedLongOp = !index;
  return 0;
}

// NewDEserverDarkRef
int CMacCmd::NewDEserverDarkRef(void)
{
  CString report;

  if (mWinApp->mGainRefMaker->MakeDEdarkRefIfNeeded(mItemInt[1], mItemFlt[2],
    report))
    ABORT_NOLINE(CString("Cannot make a new dark reference in DE server with "
    "NewDEserverDarkRef:\n") + report);
  return 0;
}

// RunInShell
int CMacCmd::RunInShell(void)
{
  SubstituteLineStripItems(mStrLine, 1, mEnteredName);
  mProcessThread = AfxBeginThread(RunInShellProc, &mEnteredName, THREAD_PRIORITY_NORMAL,
     0, CREATE_SUSPENDED);
  mProcessThread->m_bAutoDelete = false;
  mProcessThread->ResumeThread();
  return 0;
}

// RunProcess
int CMacCmd::RunProcess(void)
{
  SubstituteLineStripItems(mStrLine, 1, mEnteredName);
  if (mWinApp->mExternalTools->RunCreateProcess(mEnteredName, mNextProcessArgs, true, 
    mInputToNextProcess))
    ABORT_LINE("Error creating process for line:\n\n");
  mNextProcessArgs = "";
  mInputToNextProcess = "";
  mRanExtProcess = true;
  return 0;
}

// NextProcessArgs
int CMacCmd::NextProcessArgs(void)
{
  SubstituteLineStripItems(mStrLine, 1, mNextProcessArgs);
  return 0;
}

// PipeToNextProcess
int CMacCmd::PipeToNextProcess(void)
{
  int ind;
  Variable *var = LookupVariable(mStrItems[1], ind);
  if (!var)
    ABORT_LINE("variable is not defined for line:\n\n");
  mInputToNextProcess = var->value;
  mInputToNextProcess.Replace("\\n", "\\r\\n");
  mInputToNextProcess + "\\r\\n";
  return 0;
}

#pragma push_macro("CreateProcess")
#undef CreateProcess

// CreateProcess
int CMacCmd::CreateProcess(void)
{
  int index;

  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  index = mWinApp->mExternalTools->RunCreateProcess(mStrCopy, mNextProcessArgs, false, 
    mInputToNextProcess);
  mNextProcessArgs = "";
  mInputToNextProcess = "";
  if (index)
    ABORT_LINE("Script aborted due to failure to run process for line:\n\n");
  return 0;
}
#pragma pop_macro("CreateProcess")

// ExternalToolArgPlace
int CMacCmd::ExternalToolArgPlace(void)
{
  mRunToolArgPlacement = mItemInt[1];
  return 0;
}

// RunExternalTool
int CMacCmd::RunExternalTool(void)
{
  int index;

  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  index = mWinApp->mExternalTools->RunToolCommand(mStrCopy, mNextProcessArgs,
    mRunToolArgPlacement, mInputToNextProcess);
  mNextProcessArgs = "";
  mInputToNextProcess = "";
  if (index)
    ABORT_LINE("Script aborted due to failure to run command for line:\n\n");
  return 0;
}

// SaveFocus
int CMacCmd::SaveFocus(void)
{
  if (mFocusToRestore > -2.)
    ABORT_NOLINE("There is a second SaveFocus without a RestoreFocus");
  mFocusToRestore = mScope->GetFocus();
  mNumStatesToRestore++;
  return 0;
}

// RestoreFocus
int CMacCmd::RestoreFocus(void)
{
  if (mFocusToRestore < -2.)
    ABORT_NOLINE("There is a RestoreFocus, but focus was not saved or has been "
      "restored already");
  mScope->SetFocus(mFocusToRestore);
  mNumStatesToRestore--;
  mFocusToRestore = -999.;
  return 0;
}

// SaveBeamTilt
int CMacCmd::SaveBeamTilt(void)
{
  CString report;
  int index;

  index = mScope->GetProbeMode();
  if (mBeamTiltXtoRestore[index] > EXTRA_VALUE_TEST) {
    report = "There is a second SaveBeamTilt without a RestoreBeamTilt";
    if (FEIscope)
      report += " for " + CString(index ? "micro" : "nano") + "probe mode";
    ABORT_NOLINE(report);
  }
  mScope->GetBeamTilt(mBeamTiltXtoRestore[index], mBeamTiltYtoRestore[index]);
  mNumStatesToRestore++;
  return 0;
}

// RestoreBeamTilt
int CMacCmd::RestoreBeamTilt(void)
{
  CString report;
  int index;

  index = mScope->GetProbeMode();
  if (mBeamTiltXtoRestore[index] < EXTRA_VALUE_TEST && 
    mAstigXtoRestore[index] < EXTRA_VALUE_TEST) {
    report = "There is a RestoreBeamTilt, but beam tilt was not saved or has been "
      "restored already";
    if (FEIscope)
      report += " for " + CString(index ? "micro" : "nano") + "probe mode";
    ABORT_NOLINE(report);
  }
  if (mBeamTiltXtoRestore[index] > EXTRA_VALUE_TEST) {
    mScope->SetBeamTilt(mBeamTiltXtoRestore[index], mBeamTiltYtoRestore[index]);
    mNumStatesToRestore--;
    mBeamTiltXtoRestore[index] = mBeamTiltYtoRestore[index] = EXTRA_NO_VALUE;
  }
  if (mAstigXtoRestore[index] > EXTRA_VALUE_TEST) {
    mScope->SetObjectiveStigmator(mAstigXtoRestore[index], mAstigYtoRestore[index]);
    mNumStatesToRestore--;
    mAstigXtoRestore[index] = mAstigYtoRestore[index] = EXTRA_NO_VALUE;
  }
  mCompensatedBTforIS = false;
  return 0;
}

// PIEZO COMMANDS

// SelectPiezo
int CMacCmd::SelectPiezo(void)
{
  if (mWinApp->mPiezoControl->SelectPiezo(mItemInt[1], mItemInt[2])) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// ReportPiezoXY
int CMacCmd::ReportPiezoXY(void)
{
  double delISX, delISY;

  if (mWinApp->mPiezoControl->GetNumPlugins()) {
    if (mWinApp->mPiezoControl->GetXYPosition(delISX, delISY)) {
      AbortMacro();
      return 1;
    }
  } else {
    if (!mScope->GetPiezoXYPosition(delISX, delISY)) {
      AbortMacro();
      return 1;
    }
  }
  mLogRpt.Format("Piezo X/Y position is %6g, %6g", delISX, delISY);
  SetRepValsAndVars(1, delISX, delISY);
  return 0;
}

// ReportPiezoZ
int CMacCmd::ReportPiezoZ(void)
{
  double delISX;

  if (mWinApp->mPiezoControl->GetZPosition(delISX)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Piezo Z or only position is %6g", delISX);
  SetRepValsAndVars(1, delISX);
  return 0;
}

// MovePiezoXY
int CMacCmd::MovePiezoXY(void)
{
  if (mWinApp->mPiezoControl->GetNumPlugins()) {
    if (mWinApp->mPiezoControl->SetXYPosition(mItemDbl[1], mItemDbl[2],
      mItemInt[3] != 0)) {
      AbortMacro();
      return 1;
    }
    mMovedPiezo = true;
  } else {
    if (!mScope->SetPiezoXYPosition(mItemDbl[1], mItemDbl[2],
      mItemInt[3] != 0)) {
      AbortMacro();
      return 1;
    }
    mMovedStage = true;
  }
  return 0;
}

// MovePiezoZ
int CMacCmd::MovePiezoZ(void)
{
  if (mWinApp->mPiezoControl->SetZPosition(mItemDbl[1],
    mItemInt[2] != 0)) {
      AbortMacro();
      return 1;
  }
  mMovedPiezo = true;
  return 0;
}

