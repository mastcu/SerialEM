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
#include ".\MacroProcessor.h"
#include "MenuTargets.h"
#include "FocusManager.h"
#include "AutoTuning.h"
#include "ShiftManager.h"
#include "ShiftCalibrator.h"
#include "EMmontageController.h"
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
#include "Shared\autodoc.h"
#include "Shared\b3dutil.h"
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
        AbortMacro();
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

  bool inComment = false;
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
    cI = mConsetNums.back();
    mConSets[cI] = mConsetsSaved.back();
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

  cIndex = mTestMontError ? 1 : 0;
  if (mControl->limitScaleMax && mImBufs[cIndex].mImage && mImBufs[cIndex].mImageScale &&
    mTestScale && mImBufs[cIndex].mImageScale->GetMaxScale() < mControl->scaleMaxLimit) {
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
    mImBufs->mImage->getSize(cSizeX, cSizeY);
    cIx0 = (cSizeX - mCropXafterShot) / 2;
    cIy0 = (cSizeY - mCropYafterShot) / 2;
    cIx1 = cIx0 + mCropXafterShot - 1;
    cIy1 = cIy0 + mCropYafterShot - 1;
    mCropXafterShot = -1;
    cIx0 = mProcessImage->CropImage(mImBufs, cIy0, cIx0, cIy1, cIx1);
    if (cIx0) {
      cReport.Format("Error # %d attempting to crop new image to match buffer", cIx0);
      ABORT_NOLINE(cReport);
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
      mParseQuotes))
      ABORT_LINE("Too many items on line in script: \n\n");
    if (mStrItems[0].Find("/*") == 0) {
      mStrItems[0] = "";
      inComment = true;
      continue;
    }
    if (mVerbose > 0)
      mWinApp->AppendToLog("COMMAND: " + mStrLine, LOG_OPEN_IF_CLOSED);
    mStrItems[0].MakeUpper();
  }

  // Convert a single number to a DOMACRO (obsolete and bad!)
  InsertDomacro(&mStrItems[0]);

  // Substitute variables in parsed items and check for control word substitution
  cReport = mStrItems[0];
  if (SubstituteVariables(mStrItems, MAX_MACRO_TOKENS, mStrLine)) {
    AbortMacro();
    return 1;
  }
  mStrItems[0].MakeUpper();
  mItem1upper = mStrItems[1];
  mItem1upper.MakeUpper();
  if (cReport != mStrItems[0] && WordIsReserved(mStrItems[0]))
    ABORT_LINE("You cannot make a command by substituting a\n"
      "variable value that is a control command on line: \n\n");

  mCmdIndex = LookupCommandIndex(mStrItems[0]);

  // Do arithmetic on selected commands
  if (mCmdIndex >= 0 && ArithmeticIsAllowed(mStrItems[0])) {
    if (SeparateParentheses(&mStrItems[1], MAX_MACRO_TOKENS - 1))
      ABORT_LINE("Too many items on line after separating out parentheses in line: \n\n");
    EvaluateExpression(&mStrItems[1], MAX_MACRO_TOKENS - 1, mStrLine, 0, cIndex, cIndex2);
    for (cI = cIndex + 1; cI <= cIndex2; cI++)
      mStrItems[cI] = "";
    cIndex = CheckLegalCommandAndArgNum(&mStrItems[0], mCmdIndex, mStrLine,mCurrentMacro);
    if (cIndex) {
      if (cIndex == 1)
        ABORT_LINE("There is no longer a legal command after evaluating arithmetic on "
          "this line:\n\n");
      AbortMacro();
      return 1;
    }
  }

  // Evaluate emptiness, ints and doubles
  for (cI = 0; cI < MAX_MACRO_TOKENS; cI++) {
    mItemEmpty[cI] = mStrItems[cI].IsEmpty();
    mItemInt[cI] = 0;
    mItemDbl[cI] = 0.;
    if (!mItemEmpty[cI]) {
      mItemInt[cI] = atoi((LPCTSTR)mStrItems[cI]);
      mItemDbl[cI] = atof((LPCTSTR)mStrItems[cI]);
      mLastNonEmptyInd = cI;
    }
    mItemFlt[cI] = (float)mItemDbl[cI];
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
  mLoadingMap = false;
  mLoopInOnIdle = false;
}

// ScriptEnd, Exit, Return, EndFunction, Function
int CMacCmd::ScriptEnd(void)
{
  if (!mCallLevel || CMD_IS(EXIT) || (CMD_IS(ENDFUNCTION) && mExitAtFuncEnd)) {
    if (CMD_IS(EXIT)) {
      if (mRunningScrpLang) {
        if (!mItemEmpty[1] && mItemInt[1]) {
          mScrpLangData.errorOccurred = SCRIPT_EXIT_NO_EXC;
          mScrpLangData.exitedFromWrapper = mItemInt[1] < 0;
        } else
          mScrpLangData.errorOccurred = SCRIPT_NORMAL_EXIT;
        if (!mItemEmpty[2])
          mWinApp->AppendToLog(mStrItems[2]);
      } else if (!mStrItems[1].IsEmpty()) {
        SubstituteLineStripItems(mStrLine, 1, mStrCopy);
        mWinApp->AppendToLog(mStrCopy);
      }
    }
    AbortMacro();
    mLastCompleted = !mExitAtFuncEnd;
    if (mLastCompleted && mStartNavAcqAtEnd)
      mWinApp->AddIdleTask(TASK_START_NAV_ACQ, 0, 0);
    return 1;
  }

  // For a return, pop any loops, clear index variables
  LeaveCallLevel(CMD_IS(RETURN));
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
  if (mRunningScrpLang) {
    cIndex = ((mItemEmpty[1] && mKeyPressed == 'B') ||
      (mStrItems[1].GetLength() == 1 && mItem1upper.GetAt(0) == mKeyPressed)) ? 1 : 0;
    if (cIndex)
      mKeyPressed = 0;
    SetOneReportedValue(cIndex, 1);
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
    cIndex = 2;
  } else {
    mLoopCount[mBlockLevel] = mItemInt[2];
    mLoopLimit[mBlockLevel] = mItemInt[3];
    if (!mItemEmpty[4]) {
      mLoopIncrement[mBlockLevel] = mItemInt[4];
      if (!mItemInt[4])
        ABORT_LINE("The loop increment is 0 at line:\n\n");
    }
    cIndex = 1;
  }
  mLastIndex = -1;
  if ((mLoopIncrement[mBlockLevel] < 0 ? - 1 : 1) *
    (mLoopCount[mBlockLevel] - mLoopLimit[mBlockLevel]) > 0) {
    if (SkipToBlockEnd(SKIPTO_ENDLOOP, mStrLine)) {
      AbortMacro();
      return 1;
    }
  } else if (!mItemEmpty[cIndex]) {
    if (SetVariable(mStrItems[cIndex], 1.0, VARTYPE_INDEX, mBlockLevel, true, &cReport))
      ABORT_LINE(cReport + " in script line: \n\n");
  }
  return 0;
}

// If, ZeroLoopElseIf
int CMacCmd::If(void)
{

  // IF statement: evaluate it, go up one block level with 0 limit
  if (EvaluateIfStatement(&mStrItems[1], MAX_MACRO_TOKENS - 1, mStrLine, cTruth)) {
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
  if (!cTruth) {
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

  if (SkipToBlockEnd(SKIPTO_ENDLOOP, mStrLine, &cIndex, &cIndex2)) {
    AbortMacro();
    return 1;
  }
  mLastIndex = -1;

  // Pop any IFs and TRYs on the loop stack
  while (mBlockLevel >= 0 && mLoopLimit[mBlockLevel] <= LOOP_LIMIT_FOR_TRY) {
    mBlockLevel--;
    mBlockDepths[mCallLevel]--;
  }
  mTryCatchLevel += cIndex2;
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
  if (SkipToLabel(mItem1upper, mStrLine, cIndex, cIndex2)) {
    AbortMacro();
    return 1;
  }

  // Pop proper number of ifs and loops from stack
  for (cI = 0; cI < cIndex && mBlockLevel >= 0; cI++) {
    ClearVariables(VARTYPE_INDEX, mCallLevel, mBlockLevel);
    mBlockLevel--;
    mBlockDepths[mCallLevel]--;
  }
  mTryCatchLevel += cIndex2;
  mLastIndex = -1;
  return 0;
}

// DoMacro, DoScript, CallMacro, CallScript, Call, CallFunction, CallStringArray
int CMacCmd::DoMacro(void)
{

  // Skip any of these operations if we are in a termination function
  if (!mExitAtFuncEnd) {

    // Calling a macro or function: rely on initial error checks, get the number
    cIndex2 = 0;
    cFunc = NULL;
    if (CMD_IS(CALLFUNCTION)) {
      cFunc = FindCalledFunction(mStrLine, false, cIndex, cIx0);
      if (!cFunc)
        AbortMacro();
      cIndex2 = cFunc->startIndex;
      if (cFunc->wasCalled)
        ABORT_LINE("Trying to call a function already being called in line: \n\n");
      cFunc->wasCalled = true;
    } else if (CMD_IS(CALL)) {
      cIndex = FindCalledMacro(mStrLine, false);
    } else if (CMD_IS(CALLSTRINGARRAY)) {
      cIndex = MakeNewTempMacro(mStrItems[1], mStrItems[2], true, mStrLine);
      if (!cIndex)
        return 1;
      cIx0 = 0;
      if (CheckBlockNesting(cIndex, -1, cIx0)) {
        AbortMacro();
        return 1;
      }

    } else {
      cIndex = mItemInt[1] - 1;
    }

    // Save the current index at this level and move up a level
    // If doing a function with string arg, substitute in line for that first so the
    // pre-existing level applies
    if (!(CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT))) {
      if (mCallLevel >= MAX_CALL_DEPTH)
        ABORT_LINE("Trying to call too many levels of scripts/functions in line: \n\n");
      if (cFunc && cFunc->ifStringArg)
        SubstituteVariables(&mStrLine, 1, mStrLine);
      mCallIndex[mCallLevel++] = mCurrentIndex;
      mCallMacro[mCallLevel] = cIndex;
      mBlockDepths[mCallLevel] = -1;
      mCallFunction[mCallLevel] = cFunc;
    } else {
      mNumRuns++;
    }

    // Doing another macro (or this one over), just reset macro # and index
    mCurrentMacro = cIndex;
    mCurrentIndex = cIndex2;
    mLastIndex = -1;
    if (cFunc)
      SetVariable("FUNCTIONNAME", cFunc->name, VARTYPE_LOCAL, -1, false);

    // Create argument variables now that level is raised
    if (cFunc && (cFunc->numNumericArgs || cFunc->ifStringArg)) {
      cTruth = false;
      for (cIndex = 0; cIndex < cFunc->numNumericArgs; cIndex++) {
        cReport = cFunc->argNames[cIndex];
        if (SetVariable(cFunc->argNames[cIndex], mItemEmpty[cIndex + cIx0] ?
          0. : mItemDbl[cIndex + cIx0], VARTYPE_LOCAL, -1, false, &cReport)) {
            cTruth = true;
            break;
        }
      }
      if (!cTruth && cFunc->ifStringArg) {
        mParamIO->StripItems(mStrLine, cIndex + cIx0, mStrCopy);
        cTruth = SetVariable(cFunc->argNames[cIndex], mStrCopy, VARTYPE_LOCAL,
          - 1, false, &cReport);
      }
      if (cTruth)
        ABORT_LINE("Failed to set argument variables in function call:\r\n" + cReport +
        " in line:\n\n")
      for (cIndex = 0; cIndex < cFunc->numNumericArgs + (cFunc->ifStringArg ? 1 : 0);
        cIndex++)
          if (mItemEmpty[cIndex + cIx0])
            break;
      SetVariable("NUMCALLARGS", cIndex, VARTYPE_LOCAL, -1, false);
   }
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
  cIndex = MakeNewTempMacro(mStrItems[1], mStrItems[2], false, mStrLine);
  if (!cIndex)
    return 1;
  return 0;
}

// OnStopCallFunc
int CMacCmd::OnStopCallFunc(void)
{
  if (!mExitAtFuncEnd) {
    cFunc = FindCalledFunction(mStrLine, false, mOnStopMacroIndex, cIx0);
    if (!cFunc) {
      AbortMacro();
      return 1;
    }
    mOnStopLineIndex = cFunc->startIndex;
    if (cFunc->numNumericArgs || cFunc->ifStringArg)
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
  SetVariable("TESTRESULT", mLastTestResult ? 1. : 0., VARTYPE_REGULAR, -1, false);
  return 0;
}

// SetVariable, AppendToArray
int CMacCmd::SetVariableCmd(void)
{
  if (mRunningScrpLang) {
    cIndex = CMD_IS(SETVARIABLE) ? VARTYPE_REGULAR : VARTYPE_PERSIST;
    if (SetVariable(mItem1upper, mStrItems[2], cIndex, -1, false, &cReport))
      ABORT_LINE(cReport + " in script line: \n\n");
    return 0;
   }

  // Do assignment to variable before any non-reserved commands
  cIndex2 = 2;
  cIx0 = CheckForArrayAssignment(mStrItems, cIndex2);
  if (SeparateParentheses(&mStrItems[cIndex2], MAX_MACRO_TOKENS - cIndex2))
    ABORT_LINE("Too many items on line after separating out parentheses in line: \n\n");
  if (EvaluateExpression(&mStrItems[cIndex2], MAX_MACRO_TOKENS - cIndex2, mStrLine, cIx0,
    cIndex, cIx1)) {
      AbortMacro();
      return 1;
  }

  // Concatenate array elements separated by \n
  if (cIx0 > 0) {
    for (cIx1 = cIndex2 + 1; cIx1 < MAX_MACRO_TOKENS; cIx1++) {
      if (mStrItems[cIx1].IsEmpty())
        break;
      mStrItems[cIndex2] += "\n" + mStrItems[cIx1];
    }
  }

  // Set the variable
  if (CMD_IS(SETVARIABLE)) {
    cIndex = mStrItems[1] == "=" ? VARTYPE_REGULAR : VARTYPE_PERSIST;
    if (SetVariable(mStrItems[0], mStrItems[cIndex2], cIndex, -1, false, &cReport))
      ABORT_LINE(cReport + " in script line: \n\n");

  } else {

    // Append to array: Split off an index to look it up
    ArrayRow newRow;

    cVar = GetVariableValuePointers(mItem1upper, &cValPtr, &cNumElemPtr, "append to",
      cReport);
    if (!cVar)
      ABORT_LINE(cReport + " for line:\n\n");
    cTruth = cValPtr == NULL;
    if (cTruth) {
      cValPtr = &newRow.value;
      cNumElemPtr = &newRow.numElements;
    }

    // Add the string to the value and compute the number of elements
    if (!cValPtr->IsEmpty())
      *cValPtr += "\n";
    *cValPtr += mStrItems[cIndex2];
    *cNumElemPtr = 1;
    cIx0 = -1;
    while ((cIx0 = cValPtr->Find('\n', cIx0 + 1)) >= 0)
      *cNumElemPtr += 1;

    // Both 1D array and existing row of 2D array should be done: add new row to 2D
    if (cTruth)
      cVar->rowsFor2d->Add(newRow);
  }
  return 0;
}

// GetVariable
int CMacCmd::GetVariable(void)
{
  cVar = LookupVariable(mStrItems[1], cIx0);
  if (!cVar)
    ABORT_LINE("The variable " + mStrItems[1] + " is not defined in line:\n\n");
  SetOneReportedValue(cVar->value, 1);
  return 0;
}

// TruncateArray
int CMacCmd::TruncateArray(void)
{
  cIndex = mItemInt[2];
  if (cIndex < 1)
    ABORT_LINE("The number to truncate to must be at least 1 for line:\n\n");
  cVar = GetVariableValuePointers(mItem1upper, &cValPtr, &cNumElemPtr, "truncate", 
    cReport);
  if (!cVar)
    ABORT_LINE(cReport + " for line:\n\n");
  if (!cValPtr) {
    if (cVar->rowsFor2d->GetSize() > cIndex)
      cVar->rowsFor2d->SetSize(cIndex);
  } else if (*cNumElemPtr > cIndex) {
    FindValueAtIndex(*cValPtr, cIndex, cIx0, cIx1);
    *cValPtr = cValPtr->Left(cIx1);
    *cNumElemPtr = cIndex;
  }
  return 0;
}

// ArrayStatistics
int CMacCmd::ArrayStatistics(void)
{
  float *fvalues = FloatArrayFromVariable(mItem1upper, cIndex2, cReport);
  if (!fvalues)
    ABORT_LINE(cReport + " in line\n\n");
  cBmin = 1.e37f;
  cBmax = -1.e37f;
  for (cIndex = 0; cIndex < cIndex2; cIndex++) {
    ACCUM_MIN(cBmin, fvalues[cIndex]);
    ACCUM_MAX(cBmax, fvalues[cIndex]);
  }
  avgSD(fvalues, cIndex2, &cBmean, &cBSD, &cCpe);
  rsFastMedianInPlace(fvalues, cIndex2, &cCpe);
  mLogRpt.Format("n= %d  min= %.6g  max = %.6g  mean= %.6g  sd= %.6g  median= %.6g",
    cIndex2, cBmin, cBmax, cBmean, cBSD, cCpe);
  SetReportedValues(&mStrItems[2], cIndex2, cBmin, cBmax, cBmean, cBSD, cCpe);

  delete[] fvalues;
  return 0;
}

// LinearFitToVars
int CMacCmd::LinearFitToVars(void)
{
  float *xx1, *xx2, *xx3 = NULL;
  cTruth = !mItemEmpty[3];
  xx1 = FloatArrayFromVariable(mItem1upper, cIndex, cReport);
  if (!xx1)
    ABORT_LINE(cReport + " in line\n\n");
  xx2 = FloatArrayFromVariable(mStrItems[2], cIndex2, cReport);
  if (!xx2) {
    delete[] xx1;
    ABORT_LINE(cReport + " in line\n\n");
  }
  if (cTruth) {
    xx3 = FloatArrayFromVariable(mStrItems[3], cIx0, cReport);
    if (!xx3) {
      delete[] xx1, xx2;
      ABORT_LINE(cReport + " in line\n\n");
    }
  }
  if (cIndex != cIndex2 || (cTruth && cIx0 != cIndex)) {
    delete[] xx1, xx2, xx3;
    ABORT_LINE("Variables do not have the same number of elements in line:\n\n");
  }
  if (cIndex < 2 || (cTruth && cIndex < 3)) {
    delete[] xx1, xx2, xx3;
    ABORT_LINE("There are not enough array values for a linear fit in line:\n\n");
  }
  if (cTruth) {
    lsFit2(xx1, xx2, xx3, cIndex, &cBmean, &cBSD, &cCpe);
    mLogRpt.Format("n= %d  a1= %f  a2= %f  c= %f", cIndex, cBmean, cBSD, cCpe);
  } else {
    lsFit(xx1, xx2, cIndex, &cBmean, &cBSD, &cCpe);
    mLogRpt.Format("n= %d  slope= %f  intercept= %f  ro= %.4f", cIndex, cBmean, cBSD, 
      cCpe);
  }
  SetReportedValues(cIndex, cBmean, cBSD, cCpe);
  delete[] xx1, xx2, xx3;
  return 0;
}

// SetStringVar
int CMacCmd::SetStringVar(void)
{
  SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  cIndex = mStrItems[1] == "@=" ? VARTYPE_REGULAR : VARTYPE_PERSIST;
  if (SetVariable(mStrItems[0], mStrCopy, cIndex, -1, false, &cReport))
    ABORT_LINE(cReport + " in script line: \n\n");
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
  cVar = LookupVariable(mStrItems[1], cIx0);
  if (!cVar)
    ABORT_LINE("The variable " + mStrItems[1] + " is not defined in line:\n\n");
  cIndex = 1;
  if (!mItemEmpty[2] && !mItemInt[2])
    cIndex = 0;
  if ((cIndex && cVar->type != VARTYPE_REGULAR) ||
    (!cIndex && cVar->type != VARTYPE_PERSIST))
    ABORT_LINE("The variable " + mStrItems[1] + " must be " +
      CString(cIndex ? "regular" : "persistent") + " to change its type in line:\n\n");
  cVar->type = cIndex ? VARTYPE_PERSIST : VARTYPE_REGULAR;
  return 0;
}

// IsVariableDefined
int CMacCmd::IsVariableDefined(void)
{
  cIndex = B3DCHOICE(LookupVariable(mItem1upper, cIndex2) != NULL, 1, 0);
  SubstituteVariables(&mStrLine, 1, mStrLine);
  mParamIO->ParseString(mStrLine, mStrItems, MAX_MACRO_TOKENS, mParseQuotes);
  mLogRpt.Format("Variable %s %s defined", (LPCTSTR)mStrItems[1],
    B3DCHOICE(cIndex, "IS", "is NOT"));
  SetReportedValues(&mStrItems[2], cIndex);
  return 0;
}

// NewArray, New2DArray
int CMacCmd::NewArrayCmd(void)
{
  CArray < ArrayRow, ArrayRow > *rowsFor2d = NULL;
  ArrayRow arrRow;

  // Process local vesus persistent property
  cIx0 = VARTYPE_REGULAR;
  if (mItemInt[2] < 0) {
    cIx0 = VARTYPE_LOCAL;
    if (LocalVarAlreadyDefined(mStrItems[1], mStrLine) > 0)
      return 1;
  }
  if (mItemInt[2] > 0)
    cIx0 = VARTYPE_PERSIST;

  // Get number of elements: 3 for 1D and 4 for 2D
  cTruth = CMD_IS(NEW2DARRAY);
  cIndex = !mItemEmpty[3] ? mItemInt[3] : 0;
  if (cTruth) {
    cIndex2 = !mItemEmpty[3] ? mItemInt[3] : 0;
    cIndex = !mItemEmpty[4] ? mItemInt[4] : 0;
    if (cIndex > 0 && !cIndex2)
      ABORT_LINE("The number of elements per row must be 0 because no rows are created"
      " in:\n\n");
  }
  if (cIndex < 0 || (cTruth && cIndex2 < 0))
    ABORT_LINE("The number of elements to create must be positive in:\n\n");
  mStrCopy = "";
  for (cIx1 = 0; cIx1 < cIndex; cIx1++)
    mStrCopy += cIx1 ? "\n0" : "0";

  // Create the 2D array rows and add string to the rows
  if (cTruth) {
    rowsFor2d = new CArray < ArrayRow, ArrayRow > ;
    arrRow.value = mStrCopy;
    arrRow.numElements = cIndex;
    for (cIy0 = 0; cIy0 < cIndex2; cIy0++)
      rowsFor2d->Add(arrRow);
  }
  if (SetVariable(mItem1upper, cTruth ? "0" : mStrCopy, cIx0, -1, false, &cReport,
    rowsFor2d)) {
    delete rowsFor2d;
    ABORT_LINE(cReport + " in script line: \n\n");
  }
  return 0;
}

// LocalVar
int CMacCmd::LocalVar(void)
{
  for (cIndex = 1; cIndex < MAX_MACRO_TOKENS && !mItemEmpty[cIndex]; cIndex++) {
    cIndex2 = LocalVarAlreadyDefined(mStrItems[cIndex], mStrLine);
    if (cIndex2 > 0)
      return 1;
    if (!cIndex2 && SetVariable(mStrItems[cIndex], "0", VARTYPE_LOCAL, mCurrentMacro, true,
      &cReport))
      ABORT_LINE(cReport + " in script line: \n\n");
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
  SubstituteVariables(&mStrLine, 1, mStrLine);
  cDelX = mWinApp->mPluginManager->ExecuteCommand(mStrLine, mItemInt, mItemDbl, 
    mItemEmpty, cReport, cDelY, cDelISX, cDelISY, cIndex2, cIndex);
  if (cIndex) {
    AbortMacro();
    return 1;
  }
  mStrCopy = "";
  if (cIndex2 == 3) {
    SetReportedValues(cDelX, cDelY, cDelISX, cDelISY);
    mStrCopy.Format(" and it returned %6g, %6g, %6g", cDelY, cDelISX, cDelISY);
  } else if (cIndex == 2) {
    SetReportedValues(cDelX, cDelY, cDelISX);
    mStrCopy.Format(" and it returned %6g, %6g", cDelY, cDelISX);
  } else if (cIndex == 1) {
    SetReportedValues(cDelX, cDelY);
    mStrCopy.Format(" and it returned %6g", cDelY);
  } else
    SetReportedValues(cDelX);
  cReport += mStrCopy;
  mWinApp->AppendToLog(cReport, mLogAction);
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
  cDelX = 0.3;
  cIndex2 = 4;
  cI = RGB(192, 192, 0);
  if (mItemInt[1] > 0)
    cIndex2 = mItemInt[1];
  if (!mItemEmpty[2] && mItemDbl[2] >= 0.001)
    cDelX = mItemDbl[2];
  if (!mItemEmpty[3] && mStrItems[3].GetLength() == 6)
    if (sscanf(mStrItems[3], "%x", &cIx0) == 1)
      cI = ((cIx0 & 0xFF0000) >> 16) | (cIx0 & 0x00FF00) | ((cIx0 & 0x0000FF) << 16);
  if (cDelX * cIndex2 > 3)
    ABORT_LINE("Flashing duration is too long in line:\r\n\r\n");
  for (cIndex = 0; cIndex < cIndex2; cIndex++) {
    mWinApp->mMainView->SetFlashNextDisplay(true);
    mWinApp->mMainView->SetFlashColor(cI);
    mWinApp->mMainView->DrawImage();
    Sleep(B3DNINT(1000. *cDelX));
    mWinApp->mMainView->DrawImage();
    if (cIndex < cIndex2 - 1)
      Sleep(B3DNINT(1000. *cDelX));
  }
  return 0;
}

// TiltUp, U
int CMacCmd::TiltUp(void)
{

  // For tilting, if stage is ready, do the action; otherwise back up the index
  if (mScope->StageBusy() <= 0) {
    SetIntensityFactor(1);
    SetupStageRestoreAfterTilt(&mStrItems[1], cDelISX, cDelISY);
    mScope->TiltUp(cDelISX, cDelISY);
    mMovedStage = true;
    mTestTiltAngle = true;
  } else
    mLastIndex = mCurrentIndex;
  return 0;
}

// TiltDown, D
int CMacCmd::TiltDown(void)
{
  if (mScope->StageBusy() <= 0) {
    SetIntensityFactor(-1);
    SetupStageRestoreAfterTilt(&mStrItems[1], cDelISX, cDelISY);
    mScope->TiltDown(cDelISX, cDelISY);
    mMovedStage = true;
    mTestTiltAngle = true;
  } else
    mLastIndex = mCurrentIndex;
  return 0;
}

// TiltTo, TiltBy
int CMacCmd::TiltTo(void)
{
  double angle = mItemFlt[1];
  if (CMD_IS(TILTBY))
    angle += mScope->GetTiltAngle();
  if (fabs(angle) > mScope->GetMaxTiltAngle() + 0.05) {
    cReport.Format("Attempt to tilt too high in script, to %.1f degrees, in line:\n\n",
      angle);
    ABORT_LINE(cReport);
  }
  if (mScope->StageBusy() <= 0) {
    SetupStageRestoreAfterTilt(&mStrItems[2], cDelISX, cDelISY);
    mScope->TiltTo(angle, cDelISX, cDelISY);
    mMovedStage = true;
    mTestTiltAngle = true;
  }
  else
    mLastIndex = mCurrentIndex;
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
  cTruth = CMD_IS(PRECOOKMONTAGE);
  cIndex = MONT_NOT_TRIAL;
  if (cTruth)
    cIndex = MONT_TRIAL_PRECOOK;
  else if (mItemInt[1] != 0)
    cIndex = MONT_TRIAL_IMAGE;
  if (!mWinApp->Montaging())
    ABORT_NOLINE("The script contains a montage statement and \n"
      "montaging is not activated");
  if (mWinApp->mMontageController->StartMontage(cIndex, false,
    (float)(cTruth ? mItemDbl[1] : 0.), (cTruth && !mItemEmpty[2]) ? mItemInt[2] : 0,
    cTruth && mItemInt[3] != 0,
    (cTruth && !mItemEmpty[4]) ? mItemFlt[4] : 0.f))
    AbortMacro();
  mTestMontError = !cTruth;
  mTestScale = !cTruth;
  return 0;
}

// OppositeTrial, OppositeFocus, OppositeAutoFocus
int CMacCmd::OppositeTrial(void)
{
  if (!mWinApp->LowDoseMode())
    ABORT_LINE("Low dose mode needs to be on to use opposite area in statement: \n\n");
  if (CMD_IS(OPPOSITEAUTOFOCUS) && !mWinApp->mFocusManager->FocusReady())
    ABORT_NOLINE("because autofocus not calibrated");
  if (mCamera->OppositeLDAreaNextShot())
    ABORT_LINE("You can not use opposite areas when Balance Shifts is on in "
    "statement: \n\n");
  mTestScale = true;
  if (CMD_IS(OPPOSITEAUTOFOCUS)) {
    mWinApp->mFocusManager->SetUseOppositeLDArea(true);
    cIndex = mItemEmpty[1] ? 1 : mItemInt[1];
    mWinApp->mFocusManager->AutoFocusStart(cIndex);
  } else if (CMD_IS(OPPOSITETRIAL))
    mCamera->InitiateCapture(2);
  else
    mCamera->InitiateCapture(1);
  return 0;
}

// AcquireToMatchBuffer
int CMacCmd::AcquireToMatchBuffer(void)
{
  if (ConvertBufferLetter(mStrItems[1], -1, true, cIndex, cReport))
    ABORT_LINE(cReport);
  cIndex2 = mImBufs[cIndex].mConSetUsed;
  cBinning = mImBufs[cIndex].mBinning;
  if (cIndex2 < 0 || cBinning <= 0)
    ABORT_LINE("Either the parameter set or the binning is not specified in the buffer"
      " for:\n\n");
  if (mImBufs[cIndex].mCamera != mCurrentCam)
    ABORT_LINE("Current camera is no longer the same as was used to acquire the image "
      "for:\n\n");
  if (cIndex2 == MONTAGE_CONSET)
    cIndex2 = RECORD_CONSET;
  if (cIndex2 == TRACK_CONSET)
    cIndex2 = TRIAL_CONSET;
  mImBufs[cIndex].mImage->getSize(mCropXafterShot, mCropYafterShot);
  cSizeX = mCropXafterShot;
  cSizeY = mCropYafterShot;
  cV1 = 1.05;

  // Get the parameters to achieve a size that is big enough
  for (;;) {
    mCamera->CenteredSizes(cSizeX, mCamParams->sizeX, mCamParams->moduloX, cIx0, cIx1,
      cSizeY, mCamParams->sizeY, mCamParams->moduloY, cIy0, cIy1, cBinning);
    if (cSizeX >= mCamParams->sizeX / cBinning && cSizeY >= mCamParams->sizeY / cBinning)
      break;
    if (cSizeX >= mCropXafterShot && cSizeY >= mCropYafterShot)
      break;
    if (cSizeX < mCropXafterShot)
      cSizeX = B3DMIN((int)(cV1 * mCropXafterShot), mCamParams->sizeX / cBinning);
    if (cSizeY < mCropYafterShot)
      cSizeY = B3DMIN((int)(cV1 * mCropYafterShot), mCamParams->sizeY / cBinning);
    cV1 *= 1.05;
  }

  // Save the control set on top of stack and make modifications
  mConsetsSaved.push_back(mConSets[cIndex2]);
  mConsetNums.push_back(cIndex2);
  mRestoreConsetAfterShot = true;
  mConSets[cIndex2].binning = cBinning;
  mConSets[cIndex2].left = cIx0 * cBinning;
  mConSets[cIndex2].right = cIx1 * cBinning;
  mConSets[cIndex2].top = cIy0 * cBinning;
  mConSets[cIndex2].bottom = cIy1 * cBinning;
  mConSets[cIndex2].exposure = mImBufs[cIndex].mExposure;
  mConSets[cIndex2].K2ReadMode = mImBufs[cIndex].mK2ReadMode;
  mConSets[cIndex2].mode = SINGLE_FRAME;
  mConSets[cIndex2].alignFrames = 0;
  mConSets[cIndex2].saveFrames = 0;

  // Cancel crop if it isn't needed
  if (cSizeX == mCropXafterShot && cSizeY == mCropYafterShot)
    mCropXafterShot = -1;
  mCamera->InitiateCapture(cIndex2);
  return 0;
}

// StepFocusNextShot
int CMacCmd::StepFocusNextShot(void)
{
  cDelX = 0.;
  cDelY = 0.;
  if (!mItemEmpty[4]) {
    cDelX = mItemDbl[3];
    cDelY = mItemDbl[4];
  }
  if (mItemDbl[1] <= 0. || cDelX < 0. || (cDelX > 0. && cDelX < mItemDbl[1]) ||
    (!cDelX && !mItemEmpty[3]))
    ABORT_LINE("Negative time, times out of order, or odd number of values in "
    "statement: \n\n");
  mCamera->QueueFocusSteps(mItemFlt[1], mItemDbl[2], (float)cDelX, cDelY);
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
  cIndex = mItemInt[1];
  if (cIndex < 0)
    cIndex = 65535;
  if (mCamera->SetNextAsyncSumFrames(cIndex, mItemInt[2] > 0)) {
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
  mCamera->SetNextFrameSkipThresh(mItemFlt[1]);
  if (!mItemEmpty[2]) {
    cBacklashX = mItemFlt[2];
    cBacklashY = mItemEmpty[3] ? cBacklashX : mItemFlt[3];
    if (cBacklashX >= 1. || cBacklashX < 0. || cBacklashY >= 1. || cBacklashY < 0)
      ABORT_LINE("Partial frame thresholds for Alignframes must be >= 0 and < 1 for "
        "line:\n\n");
    mCamera->SetNextPartialThresholds(cBacklashX, cBacklashY);
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
  FloatVec openTime, tiltToAngle, waitOrInterval, focusChange, deltaISX, deltaISY;

  // Get matrix for image shift conversion
  cIndex = mWinApp->LowDoseMode() ? mLdParam[RECORD_CONSET].magIndex :
    mScope->GetMagIndex();
  cAMat = MatMul(MatInv(mShiftManager->CameraToSpecimen(cIndex)),
    mShiftManager->CameraToIS(cIndex));

  // Set up series based on specifications
  if (CMD_IS(QUEUEFRAMETILTSERIES)) {
    cDelISX = mItemEmpty[4] ? 0. : mItemDbl[4];
    for (cIndex = 0; cIndex < mItemInt[3]; cIndex++) {
      tiltToAngle.push_back((float)(mItemDbl[1] + mItemDbl[2] * cIndex));
      if (!mItemEmpty[5])
        openTime.push_back(mItemFlt[5]);
      if (!mItemEmpty[6])
        waitOrInterval.push_back(mItemFlt[6]);
      if (!mItemEmpty[7])
        focusChange.push_back((float)(mItemDbl[7] *cIndex));
      if (!mItemEmpty[9]) {
        deltaISX.push_back((float)((mItemDbl[8] *cAMat.xpx + mItemDbl[9] * cAMat.xpy) *
          cIndex));
        deltaISY.push_back((float)((mItemDbl[8] *cAMat.ypx + mItemDbl[9] * cAMat.ypy) *
          cIndex));
      }
    }

  } else {

    // Or pull the values out of the big variable array: figure out how many per step
    cDelISX = mItemEmpty[3] ? 0. : mItemDbl[3];
    cVar = LookupVariable(mItem1upper, cIndex2);
    if (!cVar)
      ABORT_LINE("The variable " + mStrItems[1] + " is not defined in line:\n\n");
    cSizeX = 0;
    if (mItemInt[2] > 31 || mItemInt[2] < 1)
      ABORT_LINE("The entry with flags must be between 1 and 31 in line:\n\n");
    if (mItemInt[2] & 1)
      cSizeX++;
    if (mItemInt[2] & 2)
      cSizeX++;
    if (mItemInt[2] & 4)
      cSizeX++;
    if (mItemInt[2] & 8)
      cSizeX++;
    if (mItemInt[2] & 16)
      cSizeX += 2;
    if (cVar->rowsFor2d) {
      cSizeY = (int)cVar->rowsFor2d->GetSize();
      for (cIx0 = 0; cIx0 < cSizeY; cIx0++) {
        ArrayRow& tempRow = cVar->rowsFor2d->ElementAt(cIx0);
        if (tempRow.numElements < cSizeX) {
          cReport.Format("Row %d or the 2D array %s has only %d elements, not the %d"
            " required", cIx0, mItem1upper, tempRow.numElements, cSizeX);
          ABORT_NOLINE(cReport);
        }
      }
    } else {
      cSizeY = cVar->numElements / cSizeX;
      cValPtr = &cVar->value;
      if (cVar->numElements % cSizeX) {
        cReport.Format("Variable %s has %d elements, not divisible by the\n"
          "%d values per step implied by the flags entry of %d", mStrItems[1],
          cVar->numElements, cSizeX, mItemInt[2]);
        ABORT_NOLINE(cReport);
      }
    }

    // Load the vectors
    cIy0 = 1;
    for (cIx0 = 0; cIx0 < cSizeY; cIx0++) {
      if (cVar->rowsFor2d) {
        ArrayRow& tempRow = cVar->rowsFor2d->ElementAt(cIx0);
        cValPtr = &tempRow.value;
        cIy0 = 1;
      }
      if (mItemInt[2] & 1) {
        FindValueAtIndex(*cValPtr, cIy0, cIx1, cIy1);
        cReport = cValPtr->Mid(cIx1, cIy1 - cIx1);
        tiltToAngle.push_back((float)atof((LPCTSTR)cReport));
        cIy0++;
      }
      if (mItemInt[2] & 2) {
        FindValueAtIndex(*cValPtr, cIy0, cIx1, cIy1);
        cReport = cValPtr->Mid(cIx1, cIy1 - cIx1);
        openTime.push_back((float)atof((LPCTSTR)cReport));
        cIy0++;
      }
      if (mItemInt[2] & 4) {
        FindValueAtIndex(*cValPtr, cIy0, cIx1, cIy1);
        cReport = cValPtr->Mid(cIx1, cIy1 - cIx1);
        waitOrInterval.push_back((float)atof((LPCTSTR)cReport));
        cIy0++;
      }
      if (mItemInt[2] & 8) {
        FindValueAtIndex(*cValPtr, cIy0, cIx1, cIy1);
        cReport = cValPtr->Mid(cIx1, cIy1 - cIx1);
        focusChange.push_back((float)atof((LPCTSTR)cReport));
        cIy0++;
      }
      if (mItemInt[2] & 16) {
        FindValueAtIndex(*cValPtr, cIy0, cIx1, cIy1);
        cReport = cValPtr->Mid(cIx1, cIy1 - cIx1);
        cDelX = atof((LPCTSTR)cReport);
        cIy0++;
        FindValueAtIndex(*cValPtr, cIy0, cIx1, cIy1);
        cReport = cValPtr->Mid(cIx1, cIy1 - cIx1);
        cDelY = atof((LPCTSTR)cReport);
        deltaISX.push_back((float)(cDelX *cAMat.xpx + cDelY * cAMat.xpy));
        deltaISY.push_back((float)(cDelX *cAMat.ypx + cDelY * cAMat.ypy));
        cIy0++;
      }
    }
  }

  // Queue it: This does all the error checking
  if (mCamera->QueueTiltSeries(openTime, tiltToAngle, waitOrInterval, focusChange,
    deltaISX, deltaISY, (float)cDelISX)) {
      AbortMacro();
      return 1;
  }
  return 0;
}

// SetFrameSeriesParams
int CMacCmd::SetFrameSeriesParams(void)
{
  cTruth = mItemInt[1] != 0;
  cBacklashX = 0.;
  if (!mItemEmpty[2])
    cBacklashX = mItemFlt[2];
  if (!mItemEmpty[3] && mItemEmpty[4] ||
    !BOOL_EQUIV(fabs(mItemDbl[3]) > 9000., fabs(mItemDbl[4]) > 9000.))
    ABORT_LINE("A Y position to restore must be entered when X is on line:\n\n");
  if (mCamera->SetFrameTSparams(cTruth, cBacklashX,
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
    for (cIndex = 0; cIndex < (int)angles->size(); cIndex++) {
      cReport.Format("%7.2f", angles->at(cIndex));
      if (cIndex < (int)startTime->size())
        mStrCopy.Format(" %5d %5d\n", B3DNINT(0.001 * startTime->at(cIndex) / frame),
          B3DNINT(0.001 *endTime->at(cIndex) / frame));
      else
        mStrCopy = "\n";
      csFile->WriteString((LPCTSTR)cReport + mStrCopy);
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
  cTruth = mCamera->GetTiltSumProperties(cIndex, cIndex2, cBacklashX, cIx0, cIx1);
  if (cTruth) {
    mLogRpt = "There are no more tilt sums available";
    SetReportedValues(&mStrItems[1], 1);
  } else {
    mLogRpt.Format("Tilt sum from %.1f deg, index %d, %d frames from %d to %d",
      cBacklashX, cIndex, cIndex2, cIx0, cIx1);
    SetReportedValues(&mStrItems[1], 0, cBacklashX, cIndex, cIndex2, cIx0, cIx1);
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
  FloatVec newFocus, newSecond, *newVec = &newFocus;
  cTruth = CMD_IS(REPLACEFRAMETSSHIFTS);
  for (cIndex = 1; cIndex < (cTruth ? 3 : 2); cIndex++) {
    mStrCopy = mStrItems[cIndex];
    mStrCopy.MakeUpper();
    cVar = LookupVariable(mStrCopy, cIndex2);
    if (!cVar)
      ABORT_LINE("The variable " + mStrItems[cIndex] + " is not defined in line:\n\n");
    if (cVar->rowsFor2d)
      ABORT_LINE("The variable " + mStrItems[cIndex] + " should not be a 2D array for "
        "line:\n\n");
    cValPtr = &cVar->value;
    for (cIy0 = 0; cIy0 < cVar->numElements; cIy0++) {
      FindValueAtIndex(*cValPtr, cIy0, cIx1, cIy1);
      cReport = cValPtr->Mid(cIx1, cIy1 - cIx1);
      newVec->push_back((float)atof((LPCTSTR)cReport));
    }
    if (cTruth)
      newVec = &newSecond;
  }
  if (cTruth)
    cIndex2 = mCamera->ReplaceFrameTSShifts(newFocus, newSecond);
  else
    cIndex2 = mCamera->ReplaceFrameTSFocusChange(newFocus);
  if (cIndex2)
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
  if (!mCamera->PostActionsOK(&mConSets[RECORD_CONSET]))
    ABORT_LINE("Post-exposure actions are not allowed for the current camera"
      " for line:\n\n");
  double increment = mScope->GetIncrement();
  cDoBack = false;
  cDelISX = mScope->GetTiltAngle();
  cIndex = 1;
  if (CMD_IS(RECORDANDTILTTO)) {
    cIndex = 3;
    increment = mItemDbl[1] - cDelISX;
    if (!mItemEmpty[2] && mItemDbl[2]) {
      cDoBack = true;
      cBacklashX = (float)fabs(mItemDbl[2]);
      if (increment < 0)
        cBacklashX = -cBacklashX;
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
  smiRAT.alpha = cDelISX + increment;
  smiRAT.backAlpha = cBacklashX;
  cTruth = SetupStageRestoreAfterTilt(&mStrItems[cIndex], smiRAT.x, smiRAT.y);
  mCamera->QueueStageMove(smiRAT, delay, cDoBack, cTruth);
  mCamera->InitiateCapture(RECORD_CONSET);
  mTestScale = true;
  mMovedStage = true;
  mTestTiltAngle = true;
  return 0;
}

// ArePostActionsEnabled
int CMacCmd::ArePostActionsEnabled(void)
{
  cTruth = mWinApp->ActPostExposure();
  cDoShift = mCamera->PostActionsOK(&mConSets[RECORD_CONSET]);
  mLogRpt.Format("Post-exposure actions %s allowed %sfor this camera%s",
    cTruth ? "ARE" : "are NOT", cDoShift ? "in general " : "",
    cDoShift ? " but ARE for Records currently" : "");
  SetReportedValues(&mStrItems[1], cTruth ? 1. : 0., cDoShift ? 1 : 0.);
  return 0;
}

// TiltDuringRecord
int CMacCmd::TiltDuringRecord(void)
{
  cDelX = mItemEmpty[3] ? 0. : mItemDbl[3];
  if (mCamera->QueueTiltDuringShot(mItemDbl[1], mItemInt[2], cDelX)) {
    cReport.Format("Moving the stage with variable speed requires\n"
      "FEI plugin version %d (and same for server, if relevant)\n"
      "You only have version %d and this line cannot be used:\n\n",
      FEI_PLUGIN_STAGE_SPEED, mScope->GetPluginVersion());
    ABORT_LINE(cReport);
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
  if (mNavHelper->mMultiShotDlg)
    mNavHelper->mMultiShotDlg->UpdateAndUseMSparams();
  MultiShotParams *msParams = mNavHelper->GetMultiShotParams();
  cIndex = (mItemEmpty[6] || mItemInt[6] < -8) ? msParams->doEarlyReturn : mItemInt[6];
  if (!mCamParams->K2Type)
    cIndex = 0;
  cIndex2 = msParams->inHoleOrMultiHole | (mTestNextMultiShot << 2);
  if (!mItemEmpty[9] && mItemInt[9] > -9) {
    cIndex2 = mItemInt[9];
    if (cIndex2 < 1 || cIndex2 > 11)
      ABORT_LINE("The ninth entry for doing within holes or\n"
        "in multiple holes must be between 1 and 11 in line:\n\n");
  }
  cTruth = (cIndex == 0 || msParams->numEarlyFrames != 0) ? msParams->saveRecord : false;
  cIx0 = msParams->doSecondRing ? 1 : 0;
  if (!mItemEmpty[10] && mItemInt[10] > -9)
    cIx0 = mItemInt[10] ? 1 : 0;
  cIx1 = 0;
  if (cIx0)
    cIx1 = (mItemEmpty[11] || mItemInt[11] < -8) ? msParams->numShots[1] : mItemInt[11];
  cIy1 = mWinApp->mParticleTasks->StartMultiShot(
    (mItemEmpty[1] || mItemInt[1] < -8) ? msParams->numShots[0] : mItemInt[1],
    (mItemEmpty[2] || mItemInt[2] < -8) ? msParams->doCenter : mItemInt[2],
    (mItemEmpty[3] || mItemDbl[3] < -8.) ? msParams->spokeRad[0] : mItemFlt[3], cIx1,
    (mItemEmpty[12] || mItemDbl[12] < -8.) ? msParams->spokeRad[1] : mItemFlt[12],
    (mItemEmpty[4] || mItemDbl[4] < -8.) ? msParams->extraDelay : mItemFlt[4],
    (mItemEmpty[5] || mItemInt[5] < -8) ? cTruth : mItemInt[5] != 0, cIndex,
    (mItemEmpty[7] || mItemInt[7] < -8) ? msParams->numEarlyFrames : mItemInt[7],
    (mItemEmpty[8] || mItemInt[8] < -8) ? msParams->adjustBeamTilt : mItemInt[8] != 0,
    cIndex2);
  if (cIy1 > 0) {
    AbortMacro();
    return 1;
  }
  SetReportedValues(-cIy1);
  return 0;
}

// RotateMultiShotPattern
int CMacCmd::RotateMultiShotPattern(void)
{
  if (mNavHelper->mMultiShotDlg)
    mNavHelper->mMultiShotDlg->UpdateAndUseMSparams();
  cIndex = mNavHelper->RotateMultiShotVectors(mNavHelper->GetMultiShotParams(),
    mItemFlt[1], mItemInt[2] != 0);
  if (cIndex)
    ABORT_LINE(cIndex > 1 ? "No image shift to specimen transformation available "
      "for line:\n\n" : "Selected pattern is not defined for line:/n/n");
  if (mNavHelper->mMultiShotDlg)
    mNavHelper->mMultiShotDlg->UpdateSettings();
  return 0;
}

// AutoAlign, A, AlignTo, ConicalAlignTo
int CMacCmd::AutoAlign(void)
{
  cIndex = 0;
  if (CMD_IS(ALIGNTO)  || CMD_IS(CONICALALIGNTO)) {
    if (ConvertBufferLetter(mStrItems[1], -1, true, cIndex, cReport))
      ABORT_LINE(cReport);
  }
  cDelX = 0;
  cTruth = false;
  cDoShift = true;
  if (CMD_IS(CONICALALIGNTO)) {
    cDelX = mItemDbl[2];
    cTruth = mItemInt[3] != 0;
  }
  if (CMD_IS(ALIGNTO) && mItemInt[2])
    cDoShift = false;
  mDisableAlignTrim = CMD_IS(ALIGNTO) && mItemInt[3];
  cIndex2 = mShiftManager->AutoAlign(cIndex, 0, cDoShift, cTruth, NULL, 0., 0., 
    (float)cDelX);
  mDisableAlignTrim = false;
  if (cIndex2 > 0)
    SUSPEND_NOLINE("because of failure to autoalign");
  SetReportedValues(cIndex2);
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
  if (ConvertBufferLetter(mStrItems[1], -1, true, cIndex, cReport))
    ABORT_LINE(cReport);
  if (mItemDbl[3] < 0.2 || mItemDbl[3] > 50.)
    ABORT_LINE("The angle range to search must be between 0.2 and 50 degrees in:\n\n");
  if (mNavHelper->AlignWithRotation(cIndex, mItemFlt[2], mItemFlt[3], cBmin,
    cShiftX, cShiftY))
    ABORT_LINE("Failure to autoalign in:\n\n");
  SetReportedValues(&mStrItems[4], cBmin, cShiftX, cShiftY);
  return 0;
}

// AutoFocus, G
int CMacCmd::AutoFocus(void)
{
  cIndex = 1;
  if (!mItemEmpty[1])
    cIndex = mItemInt[1];
  if (cIndex > -2 && !mWinApp->mFocusManager->FocusReady())
    SUSPEND_NOLINE("because autofocus not calibrated");
  if (mMinDeltaFocus != 0. || mMaxDeltaFocus != 0.)
    mWinApp->mFocusManager->NextFocusChangeLimits(mMinDeltaFocus, mMaxDeltaFocus);
  if (mMinAbsFocus != 0. || mMaxAbsFocus != 0.)
    mWinApp->mFocusManager->NextFocusAbsoluteLimits(mMinAbsFocus, mMaxAbsFocus);
  if (cIndex < -1)
    mWinApp->mFocusManager->DetectFocus(FOCUS_REPORT,
    !mItemEmpty[2] ? mItemInt[2] : 0);
  else
    mWinApp->mFocusManager->AutoFocusStart(cIndex,
    !mItemEmpty[2] ? mItemInt[2] : 0);
  mTestScale = true;
  return 0;
}

// BeamTiltDirection
int CMacCmd::BeamTiltDirection(void)
{
  mWinApp->mFocusManager->SetTiltDirection(mItemInt[1]);
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
  cIndex = COMA_INITIAL_ITERS;
  if (mItemInt[1])
    cIndex = mItemInt[1] > 0 ? COMA_ADD_ONE_ITER : COMA_JUST_MEASURE;
  if (mWinApp->mAutoTuning->ComaFreeAlignment(false, cIndex))
    AbortMacro();
  return 0;
}

// ZemlinTableau
int CMacCmd::ZemlinTableau(void)
{
  cIndex = 340;
  cIndex2 = 170;
  if (mItemInt[2] > 10)
    cIndex = mItemInt[2];
  if (mItemInt[3] > 0)
    cIndex2 = mItemInt[3];
  mWinApp->mAutoTuning->MakeZemlinTableau(mItemFlt[1], cIndex, cIndex2);
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
  cIndex = 0;
  if (CMD_IS(FIXCOMABYCTF)) {
     cIndex = mWinApp->mAutoTuning->GetCtfDoFullArray() ? 2 : 1;
    if (!mItemEmpty[3])
      cIndex = mItemInt[3] > 0 ? 2 : 1;
    cTruth = mItemInt[4] > 0;
  } else
    cTruth = mItemInt[3] > 0;
  if (mWinApp->mAutoTuning->CtfBasedAstigmatismComa(cIndex, false,
    (mItemInt[1] > 0) ? 1 : 0, mItemInt[2] > 0, cTruth)){
    AbortMacro();
    return 1;
  }
  return 0;
}

// ReportStigmatorNeeded
int CMacCmd::ReportStigmatorNeeded(void)
{
  mWinApp->mAutoTuning->GetLastAstigNeeded(cBacklashX, cBacklashY);
  mLogRpt.Format("Last measured stigmator change needed: %.4f  %.4f", cBacklashX,
    cBacklashY);
  SetReportedValues(&mStrItems[1], cBacklashX, cBacklashY);
  return 0;
}

// ReportComaTiltNeeded
int CMacCmd::ReportComaTiltNeeded(void)
{
  mWinApp->mAutoTuning->GetLastBeamTiltNeeded(cBacklashX, cBacklashY);
  mLogRpt.Format("Last measured beam tilt change needed: %.2f  %.2f", cBacklashX,
    cBacklashY);
  SetReportedValues(&mStrItems[1], cBacklashX, cBacklashY);
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
  SetReportedValues(&mStrItems[1], cvsis->matrix.xpx, cvsis->matrix.xpy,
    cvsis->matrix.ypx, cvsis->matrix.ypy);
  return 0;
}

// Save, S
int CMacCmd::Save(void)
{
  cIndex2 = -1;
  if (ConvertBufferLetter(mStrItems[1], 0, true, cI, cReport))
    ABORT_LINE(cReport);
  if (!mItemEmpty[2]) {
    cIndex2 = mItemInt[2] - 1;
    if (cIndex2 < 0 || cIndex2 >= mWinApp->mDocWnd->GetNumStores())
      ABORT_LINE("File # to save to is beyond range of open file numbers in "
        "statement: \n\n");
  }

  cReport = cI == 0 ? "A" : mStrItems[1];
  if (cIndex2 < 0 && (mWinApp->Montaging() ||
    !mBufferManager->IsBufferSavable(mImBufs + cI)))
    SUSPEND_NOLINE("because buffer " + cReport + " is not savable to current file");
  if (mItemEmpty[1])
    mWinApp->mDocWnd->SaveRegularBuffer();
  else if (mWinApp->mDocWnd->SaveBufferToFile(cI, cIndex2))
    SUSPEND_LINE("because of error saving to file in statement: \n\n");
  EMimageExtra *extra = (EMimageExtra * )mImBufs[cI].mImage->GetUserData();
  if (extra) {
    if (cIndex2 < 0)
      cReport.Format("Saved Z =%4d, %6.2f degrees", mImBufs[cI].mSecNumber,
        extra->m_fTilt);
    else
      cReport.Format("Saved Z =%4d, %6.2f degrees to file #%d", mImBufs[cI].mSecNumber,
        extra->m_fTilt, cIndex2 + 1);
    mWinApp->AppendToLog(cReport, LOG_SWALLOW_IF_CLOSED);
  }
  return 0;
}

// ReadFile
int CMacCmd::ReadFile(void)
{
  cIndex = mItemInt[1];
  cIy0 = mBufferManager->GetBufToReadInto();
  if (!mWinApp->mStoreMRC)
    SUSPEND_NOLINE("on ReadFile because there is no open file");
  if (ConvertBufferLetter(mStrItems[2], cIy0, false, cIx0, cReport))
    ABORT_LINE(cReport);
  mBufferManager->SetBufToReadInto(cIx0);
  if (mWinApp->Montaging())
    cIndex2 = mWinApp->mMontageController->ReadMontage(cIndex, NULL, NULL, false, true);
  else {
    cIndex2 = mBufferManager->ReadFromFile(mWinApp->mStoreMRC, cIndex);
    if (!cIndex2)
      mWinApp->DrawReadInImage();
  }
  mBufferManager->SetBufToReadInto(cIy0);
  if (cIndex2)
    ABORT_NOLINE("Script stopped because of error reading from file");
  return 0;
}

// ReadOtherFile
int CMacCmd::ReadOtherFile(void)
{
  cIndex = mItemInt[1];
  if (cIndex < 0)
    ABORT_LINE("Section to read is negative in line:\n\n");
  cIy0 = mBufferManager->GetBufToReadInto();
  if (mItemInt[2] < 0)
    cIx0 = cIy0;
  else if (ConvertBufferLetter(mStrItems[2], -1, false, cIx0, cReport))
    ABORT_LINE(cReport);
  if (CheckConvertFilename(mStrItems, mStrLine, 3, cReport))
    return 1;
  for (cIx1 = 0; cIx1 <= mRetryReadOther; cIx1++) {
    mBufferManager->SetOtherFile(cReport);
    mBufferManager->SetBufToReadInto(cIx0);
    mBufferManager->SetNextSecToRead(cIndex);
    cIndex2 = mBufferManager->RereadOtherFile(mStrCopy);
    if (!cIndex2) {
      mWinApp->DrawReadInImage();
      break;
    }
    if (cIx1 < mRetryReadOther)
      Sleep(mReadOtherSleep);
  }
  mBufferManager->SetBufToReadInto(cIy0);
  if (cIndex2)
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
  ScreenShotParams *snapParams = mWinApp->GetScreenShotParams();
  int compressions[] = {COMPRESS_NONE, COMPRESS_ZIP, COMPRESS_LZW, COMPRESS_JPEG};
  cTruth = CMD_IS(SNAPSHOTTOFILE);
  if (cTruth) {
    cIx0 = 4;
    if (mItemDbl[1] > 0. && mItemDbl[1] < 1.)
      ABORT_LINE("Factor to zoom by must be >= 1 or <= 0 in line:\n\n");
    if (mItemDbl[2] > 0. && mItemDbl[2] < 1.)
      ABORT_LINE("Factor to scale sizes by must be >= 1 or <= 0 in line:\n\n");
  } else {
    cIx0 = 2;
    if (ConvertBufferLetter(mStrItems[1], -1, true, cIndex, cReport, true))
      ABORT_LINE(cReport);
  }
  cIndex2 = -1;
  if (mStrItems[cIx0] == "MRC")
    cIndex2 = STORE_TYPE_MRC;
  else if (mStrItems[cIx0] == "HDF")
    cIndex2 = STORE_TYPE_HDF;
  else if (mStrItems[cIx0] == "TIF" || mStrItems[2] == "TIFF")
    cIndex2 = STORE_TYPE_TIFF;
  else if (mStrItems[cIx0] == "JPG" || mStrItems[2] == "JPEG")
    cIndex2 = STORE_TYPE_JPEG;
  else if (mStrItems[cIx0] != "CUR" && mStrItems[2] != "-1")
    ABORT_LINE("Second entry must be MRC, TIF, TIFF, JPG, JPEG, CUR, or -1 in line:"
      "\n\n");
  if (cTruth && (cIndex2 == STORE_TYPE_MRC || cIndex2 == STORE_TYPE_HDF))
    ABORT_LINE("A snapshot cannot be saved to an MRC or HDF file in line:\n\n");
  cIx1 = -1;
  if (mStrItems[cIx0 + 1] == "NONE")
    cIx1 = COMPRESS_NONE;
  else if (mStrItems[cIx0 + 1] == "LZW")
    cIx1 = COMPRESS_LZW;
  else if (mStrItems[cIx0 + 1] == "ZIP")
    cIx1 = COMPRESS_ZIP;
  else if (mStrItems[cIx0 + 1] == "JPG" || mStrItems[cIx0 + 1] == "JPEG")
    cIx1 = COMPRESS_JPEG;
  else if (mStrItems[cIx0 + 1] != "CUR" && mStrItems[cIx0 + 1] != "-1")
    ABORT_LINE("Third entry must be NONE, LZW, ZIP, JPG, JPEG, CUR, or -1 in line:"
      "\n\n");
  if (CheckConvertFilename(mStrItems, mStrLine, cIx0 + 2, cReport))
    return 1;
  if (cTruth) {
    if (cIndex2 < 0)
      cIndex2 = snapParams->fileType ? STORE_TYPE_JPEG : STORE_TYPE_TIFF;
    if (cIx1 < 0) {
      B3DCLAMP(snapParams->compression, 0, 3);
      cIx1 = compressions[snapParams->compression];
    }
    cIy1 = mWinApp->mActiveView->TakeSnapshot(mItemFlt[1], mItemFlt[2],
      mItemInt[3], cIndex2, cIx1, snapParams->jpegQuality, cReport);
    if (CScreenShotDialog::GetSnapshotError(cIy1, cReport)) {
      ABORT_LINE("Error taking snapshot, " + cReport + " for line:\n\n");
    }
  } else {
    cIy1 = mWinApp->mDocWnd->SaveToOtherFile(cIndex, cIndex2, cIx1, &cReport);
    if (cIy1 == 1)
      return 1;
    if (cIy1) {
      cReport.Format("Error %s file for line:\n\n", cIy1 == 2 ? "opening" : "saving to");
      ABORT_LINE(cReport);
    }
  }
  return 0;
}

// OpenNewFile, OpenNewMontage, OpenFrameSumFile
int CMacCmd::OpenNewFile(void)
{
  cIndex = 1;
  if (CMD_IS(OPENNEWMONTAGE)) {
    cIndex = 3;
    cIx0 = mItemInt[1];
    if (!cIx0)
      cIx0 = mMontP->xNframes;
    cIy0 = mItemInt[2];
    if (!cIy0)
      cIy0 = mMontP->yNframes;
    if (cIx0 < 1 || cIy0 < 1)
      ABORT_LINE("Number of montages pieces is missing or not positive"
        " in statement:\n\n");
  }
  if (CMD_IS(OPENFRAMESUMFILE)) {
    SubstituteLineStripItems(mStrLine, cIndex, mStrCopy);
    cReport = mCamera->GetFrameFilename() + mStrCopy;
    mWinApp->AppendToLog("Opening file: " + cReport, LOG_SWALLOW_IF_CLOSED);
  } else if (CheckConvertFilename(mStrItems, mStrLine, cIndex, cReport))
    return 1;

  // Check if file already exists
  if (!mOverwriteOK && CFile::GetStatus((LPCTSTR)cReport, cStatus))
    SUSPEND_NOLINE("opening new file because " + cReport + " already exists");

  if (cIndex == 1)
    cIndex2 = mWinApp->mDocWnd->DoOpenNewFile(cReport);
  else {
    mWinApp->mDocWnd->LeaveCurrentFile();
    mMontP->xNframes = cIx0;
    mMontP->yNframes = cIy0;
    cIndex2 = mWinApp->mDocWnd->GetMontageParamsAndFile(true, cIx0, cIy0, cReport);
  }
  if (cIndex2)
    SUSPEND_LINE("because of error opening new file in statement:\n\n");
  mWinApp->mBufferWindow.UpdateSaveCopy();
  return 0;
}

// SetupWaffleMontage
int CMacCmd::SetupWaffleMontage(void)
{
  if (mItemInt[1] < 2)
    ABORT_LINE("Minimum number of blocks in waffle grating must be at least 2 in:\n\n");
  cBacklashX = (float)(0.462 * mItemInt[1]);
  cSizeX = mConSets[RECORD_CONSET].right - mConSets[RECORD_CONSET].left;
  cSizeY = mConSets[RECORD_CONSET].bottom - mConSets[RECORD_CONSET].top;
  cIx0 = PiecesForMinimumSize(cBacklashX, cSizeX, 0.1f);
  cIy0 = PiecesForMinimumSize(cBacklashX, cSizeY, 0.1f);
  if (cIx0 < 2 && cIy0 < 2) {
    mWinApp->AppendToLog("No montage is needed to measure pixel size at this "
      "magnification");
    SetReportedValues(0.);
  } else {
    if (mWinApp->Montaging() && mMontP->xNframes == cIx0 && mMontP->yNframes == cIy0) {
      mMontP->magIndex = mScope->GetMagIndex();
      mWinApp->AppendToLog("Existing montage can be used to measure pixel size at "
        "this magnification");
    } else {

                 // If it is montaging already, close file
      if (mWinApp->Montaging())
        mWinApp->mDocWnd->DoCloseFile();

                 // Follow same procedure as above for opening montage file
      if (CheckConvertFilename(mStrItems, mStrLine, 2, cReport))
        return 1;
      mWinApp->mDocWnd->LeaveCurrentFile();
      mMontP->xNframes = cIx0;
      mMontP->yNframes = cIy0;
      mMontP->binning = mConSets[RECORD_CONSET].binning;
      mMontP->xFrame = cSizeX / mMontP->binning;
      mMontP->yFrame = cSizeY / mMontP->binning;
      mMontP->xOverlap = mMontP->xFrame / 10;
      mMontP->yOverlap = mMontP->yFrame / 10;
      cIndex2 = mWinApp->mDocWnd->GetMontageParamsAndFile(true, cIx0, cIy0, cReport);
      mWinApp->mBufferWindow.UpdateSaveCopy();
      if (cIndex2)
        ABORT_NOLINE("Error trying to open new montage the right size for current"
        " magnification");
      PrintfToLog("Opened file %s for %d x %d montage", (LPCTSTR)cReport, cIx0, cIy0);
    }
    mMontP->warnedMagChange = true;
    mMontP->overviewBinning = 1;
    mMontP->shiftInOverview = true;
    mWinApp->mMontageWindow.UpdateSettings();
    SetReportedValues(1.);
  }
  return 0;
}

// ReportNumMontagePieces
int CMacCmd::ReportNumMontagePieces(void)
{
  if (mWinApp->Montaging()) {
    cIndex = mMontP->xNframes * mMontP->yNframes -
      (mMontP->ignoreSkipList ? 0 : mMontP->numToSkip);
    mLogRpt.Format("%d pieces will be acquired in the current montage", cIndex);
  } else {
    cIndex = 1;
    mLogRpt = "Montaging is not used for the current file";
  }
  SetReportedValues(&mStrItems[1], cIndex);
  return 0;
}

// EnterNameOpenFile
int CMacCmd::EnterNameOpenFile(void)
{
  mStrCopy = "Enter name for new file:";
  if (!mStrItems[1].IsEmpty())
    mParamIO->StripItems(mStrLine, 1, mStrCopy);
  if (!KGetOneString(mStrCopy, mEnteredName, 100))
    SUSPEND_NOLINE("because no new file was opened");
  if (mWinApp->mDocWnd->DoOpenNewFile(mEnteredName))
    SUSPEND_NOLINE("because of error opening new file by that name");
  return 0;
}

// ChooserForNewFile
int CMacCmd::ChooserForNewFile(void)
{
  cIndex = (mItemInt[1] != 0) ? STORE_TYPE_ADOC : STORE_TYPE_MRC;
  if (mWinApp->mDocWnd->FilenameForSaveFile(mItemInt[1], NULL, mStrCopy)) {
    AbortMacro();
    return 1;
  }
  cReport = mStrItems[2];
  cReport.MakeUpper();
  if (SetVariable(cReport, mStrCopy, VARTYPE_REGULAR, -1, false))
    ABORT_NOLINE("Error setting variable " + mStrItems[2] + " with filename " + mStrCopy);
  mOverwriteOK = true;
  return 0;
}

// ReadTextFile, Read2DTextFile, ReadStringsFromFile
int CMacCmd::ReadTextFile(void)
{
  SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  CString newVar;
  CStdioFile *csFile = NULL;
  CArray < ArrayRow, ArrayRow > *rowsFor2d = NULL;
  ArrayRow arrRow;
  cTruth = CMD_IS(READ2DTEXTFILE);
  try {
    csFile = new CStdioFile(mStrCopy, CFile::modeRead | CFile::shareDenyWrite);
  }
  catch (CFileException * perr) {
    perr->Delete();
    ABORT_NOLINE("Error opening file " + mStrCopy);
  }
  if (cTruth)
    rowsFor2d = new CArray < ArrayRow, ArrayRow > ;
  while ((cIndex = mParamIO->ReadAndParse(csFile, cReport, mStrItems,
    MAX_MACRO_TOKENS)) == 0) {
    if (cTruth)
      newVar = "";
    if (CMD_IS(READSTRINGSFROMFILE)) {
      if (newVar.GetLength())
        newVar += '\n';
      newVar += cReport;
    } else {
      for (cIndex2 = 0; cIndex2 < MAX_MACRO_TOKENS; cIndex2++) {
        if (mStrItems[cIndex2].IsEmpty())
          break;
        if (newVar.GetLength())
          newVar += '\n';
        newVar += mStrItems[cIndex2];
      }
    }
    if (cTruth && cIndex2 > 0) {
      arrRow.value = newVar;
      arrRow.numElements = cIndex2;
      rowsFor2d->Add(arrRow);
    }
  }
  delete csFile;
  if (cIndex > 0) {
    delete rowsFor2d;
    ABORT_NOLINE("Error reading text from file " + mStrCopy);
  }

  if (SetVariable(mItem1upper, cTruth ? "0" : newVar, VARTYPE_REGULAR, -1, false, 
    &cReport, rowsFor2d)) {
    delete rowsFor2d;
    ABORT_NOLINE("Error setting an array variable with text from file " + mStrCopy +
      ":\n" + cReport);
  }
  return 0;
}

// OpenTextFile
int CMacCmd::OpenTextFile(void)
{
  UINT openFlags = CFile::typeText;
  if (LookupFileForText(mItem1upper, TXFILE_MUST_NOT_EXIST, mStrLine, cIndex))
    return 1;
  cReport = mStrItems[2].MakeUpper();
  if (cReport.GetLength() > 1 || CString("RTWAO").Find(cReport) < 0)
    ABORT_LINE("The second entry must be R, T, W, A, or O in line\n\n:");
  SubstituteLineStripItems(mStrLine, 4, mStrCopy);

  // Set up flags and read/write from mode entry
  if (cReport == "R" || cReport == "T") {
    openFlags |= CFile::shareDenyNone | CFile::modeRead;
    cTruth = true;
  } else {
    cTruth = false;
    openFlags |= CFile::shareDenyWrite;
    if (cReport == "A") {
      openFlags |= CFile::modeReadWrite;
    } else {
      openFlags |= CFile::modeWrite | CFile::modeCreate;
      if (cReport == "W" && !mOverwriteOK && CFile::GetStatus((LPCTSTR)mStrCopy, cStatus))
        ABORT_LINE("File already exists and you entered W instead of O for line:\n\n");
    }
  }

  // Create new entry and try to open file; allowing  failure with 'T'
  cTxFile = new FileForText;
  cTxFile->readOnly = cTruth;
  cTxFile->ID = mItem1upper;
  cTxFile->persistent = mItemInt[3] != 0;
  cTxFile->csFile = NULL;
  try {
    cTxFile->csFile = new CStdioFile(mStrCopy, openFlags);
    if (cReport == "A")
      cTxFile->csFile->SeekToEnd();
  }
  catch (CFileException * perr) {
    perr->Delete();
    delete cTxFile->csFile;
    delete cTxFile;
    if (cReport != "T")
      ABORT_NOLINE("Error opening file " + mStrCopy);
    cTxFile = NULL;
  }

  // Add to array if success; set value for 'T'
  if (cTxFile)
    mTextFileArray.Add(cTxFile);
  if (cReport == "T")
    SetReportedValues(cTxFile ? 1 : 0);
  return 0;
}

// WriteLineToFile
int CMacCmd::WriteLineToFile(void)
{
  cTxFile = LookupFileForText(mItem1upper, TXFILE_WRITE_ONLY, mStrLine, cIndex);
  if (!cTxFile)
    return 1;

  // To allow comments to be written, there is only one required argument in initial
  // check so we need to check here
  SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  try {
    cTxFile->csFile->WriteString(mStrCopy + "\n");
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
  cTxFile = LookupFileForText(mItem1upper, TXFILE_READ_ONLY, mStrLine, cIndex);
  if (!cTxFile)
    return 1;
  mStrLine = mStrItems[2];
  cTruth = CMD_IS(READLINETOARRAY);

  // Skip blank lines, skip comment lines if reading into array
  for (;;) {
    cIndex = mParamIO->ReadAndParse(cTxFile->csFile, cReport, mStrItems,
      MAX_MACRO_TOKENS, mParseQuotes);
    if (cIndex || !mStrItems[0].IsEmpty() || (!cTruth && !cReport.IsEmpty()))
      break;
  }

  // Check error conditions
  if (cIndex > 0)
    ABORT_LINE("Error reading from file for line:\n\n");
  if (cIndex) {
    mWinApp->AppendToLog("End of file reached for file with ID " + cTxFile->ID,
      mLogAction);
  } else {

    // For array, concatenate items into report; otherwise report is all set
    if (cTruth) {
      cReport = "";
      for (cIndex2 = 0; cIndex2 < MAX_MACRO_TOKENS; cIndex2++) {
        if (mStrItems[cIndex2].IsEmpty())
          break;
        if (cReport.GetLength())
          cReport += '\n';
        cReport += mStrItems[cIndex2];
      }
    }
    if (SetVariable(mStrLine, cReport, VARTYPE_REGULAR, -1, false, &mStrCopy))
      ABORT_NOLINE("Error setting a variable with line from file:\n" + mStrCopy);
  }
  SetReportedValues(cIndex ? 0 : 1);
  return 0;
}

// CloseTextFile
int CMacCmd::CloseTextFile(void)
{
  if (!LookupFileForText(mItem1upper, TXFILE_MUST_EXIST, mStrLine, cIndex))
    return 1;
  CloseFileForText(cIndex);
  return 0;
}

// FlushTextFile
int CMacCmd::FlushTextFile(void)
{
  cTxFile = LookupFileForText(mItem1upper, TXFILE_MUST_EXIST, mStrLine, cIndex);
  if (!cTxFile)
    return 1;
  cTxFile->csFile->Flush();
  return 0;
}

// IsTextFileOpen
int CMacCmd::IsTextFileOpen(void)
{
  cTxFile = LookupFileForText(mItem1upper, TXFILE_QUERY_ONLY, mStrLine, cIndex);
  cTruth = false;
  if (!cTxFile && !mItemEmpty[2]) {

    // Check the file name if the it doesn't match ID
    char fullBuf[_MAX_PATH];
    SubstituteLineStripItems(mStrLine, 2, mStrCopy);
    if (GetFullPathName((LPCTSTR)mStrCopy, _MAX_PATH, fullBuf, NULL) > 0) {
      for (cIndex = 0; cIndex < mTextFileArray.GetSize(); cIndex++) {
        cTxFile = mTextFileArray.GetAt(cIndex);
        if (!(cTxFile->csFile->GetFilePath()).CompareNoCase(fullBuf)) {
          cTruth = true;
          break;
        }
      }
    }
    mLogRpt.Format("Text file with name %s %s open", (LPCTSTR)mStrCopy, cTruth ? "IS" :
      "is NOT");
    SetReportedValues(cTruth ? 1 : 0);
  } else {
    mLogRpt.Format("Text file with identifier %s %s open", (LPCTSTR)mItem1upper,
      cTxFile ? "IS" : "is NOT");
    SetReportedValues(cTxFile ? 1 : 0);
  }
  return 0;
}

// UserSetDirectory
int CMacCmd::UserSetDirectory(void)
{
  mStrCopy = "Choose a new current working directory:";
  if (!mStrItems[1].IsEmpty())
    mParamIO->StripItems(mStrLine, 1, mStrCopy);
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
  if (CheckConvertFilename(mStrItems, mStrLine, 1, cReport))
    return 1;
  cIndex = mWinApp->mDocWnd->OpenOldMrcCFile(&cCfile, cReport, false);
  if (cIndex == MRC_OPEN_NOERR || cIndex == MRC_OPEN_ADOC)
    cIndex = mWinApp->mDocWnd->OpenOldFile(cCfile, cReport, cIndex);
  if (cIndex != MRC_OPEN_NOERR)
    SUSPEND_LINE("because of error opening old file in statement:\n\n");
  return 0;
}

int CMacCmd::UserOpenOldFile(void)
{
  if (mWinApp->mDocWnd->DoFileOpenold())
    SUSPEND_LINE("because existing image file was not opened in line:\n\n");
  return 0;
}

// CloseFile
int CMacCmd::CloseFile(void)
{
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
  if (CheckConvertFilename(mStrItems, mStrLine, 1, cReport))
    return 1;
  try {
    CFile::Remove(cReport);
  }
  catch (CFileException * pEx) {
    pEx->Delete();
    PrintfToLog("WARNING: File %s cannot be removed", (LPCTSTR)cReport);
  }
  return 0;
}

// ReportCurrentFilename, ReportLastFrameFile, ReportNavFile
int CMacCmd::ReportCurrentFilename(void)
{
  cTruth = !CMD_IS(REPORTLASTFRAMEFILE);
  if (CMD_IS(REPORTCURRENTFILENAME)) {
    if (!mWinApp->mStoreMRC)
      SUSPEND_LINE("because there is no file open currently for statement:\n\n");
    cReport = mWinApp->mStoreMRC->getFilePath();
    mStrCopy = "Current open image file is: ";
  }
  else if (cTruth) {
    ABORT_NONAV;
    cReport = mWinApp->mNavigator->GetCurrentNavFile();
    if (cReport.IsEmpty())
      ABORT_LINE("There is no Navigator file open for:\n\n");
    mStrCopy = "Current open Navigator file is: ";
  }
  else {
    cReport = mCamera->GetPathForFrames();
    if (cReport.IsEmpty())
      ABORT_LINE("There is no last frame file name available for:\n\n");
    mStrCopy = "Last frame file is: ";
  }
  mWinApp->AppendToLog(mStrCopy + cReport, mLogAction);
  CString root = cReport;
  CString ext;
  if (mItemInt[1] && cTruth)
    UtilSplitExtension(cReport, root, ext);
  SetOneReportedValue(!cTruth ? &mStrItems[1] : NULL, root, 1);

  if (!ext.IsEmpty())
    SetOneReportedValue(ext, 2);
  if (mItemInt[1] && cTruth) {
    UtilSplitPath(root, cReport, ext);
    SetOneReportedValue(cReport, 3);
    SetOneReportedValue(ext, 4);
  }
  return 0;
}

// ReportFrameBaseName
int CMacCmd::ReportFrameBaseName(void)
{
  mStrCopy = mCamera->GetFrameBaseName();
  cIndex = mCamera->GetFrameNameFormat();
  if ((cIndex & FRAME_FILE_ROOT) && !mStrCopy.IsEmpty()) {
    mLogRpt = "The frame base name is " + mStrCopy;
    SetOneReportedValue(&mStrItems[1], 1., 1);
    SetOneReportedValue(&mStrItems[1], mStrCopy, 2);
  } else {
    mLogRpt = "The base name is not being used in frame file names";
    SetOneReportedValue(&mStrItems[1], 0., 1);
  }
  return 0;
}

// GetFileInWatchedDir, RunScriptInWatchedDir
int CMacCmd::GetFileInWatchedDir(void)
{
  WIN32_FIND_DATA findFileData;
  CStdioFile *sFile = NULL;
  CString direc, fname;
  if (CMD_IS(RUNSCRIPTINWATCHEDDIR)) {
    if (mNumTempMacros >= MAX_TEMP_MACROS)
      ABORT_LINE("No free temporary scripts available for line:\n\n");
    if (mNeedClearTempMacro >= 0)
      ABORT_LINE("When running a script from a file, you cannot run another one in"
        " line:\n\n");
  }
  if (CheckConvertFilename(mStrItems, mStrLine, 1, cReport))
    return 1;

  // If the string has no wild card and it is a directory, add *
  if (cReport.Find('*') < 0) {
    cTruth = CFile::GetStatus((LPCTSTR)cReport, cStatus);
    if (cTruth && (cStatus.m_attribute & CFile::directory))
      cReport += "\\*";
  }
  UtilSplitPath(cReport, direc, fname);
  HANDLE hFind = FindFirstFile((LPCTSTR)cReport, &findFileData);
  if (hFind == INVALID_HANDLE_VALUE) {
    SetReportedValues(0.);
    return 0;
  }
  cTruth = false;
  do {
    cReport = findFileData.cFileName;

    // If found a file, look for lock file and wait until goes away, or give up and
    // go on to next file if any
    for (cIndex = 0; cIndex < 10; cIndex++) {
      if (!CFile::GetStatus((LPCTSTR)(cReport + ".lock"), cStatus)) {
        cTruth = true;
        break;
      }
      Sleep(20);
    }
    if (cTruth)
      break;

  } while (FindNextFile(hFind, &findFileData) == 0);
  FindClose(hFind);

  // Just set report value if no file. give message if there is one
  if (!cTruth) {
    SetReportedValues(0.);
  } else {
    cReport = direc + "\\" + cReport;
    SetOneReportedValue(1., 1);
    SetOneReportedValue(cReport, 2);
    mLogRpt = "Found file " + cReport;
  }

  if (CMD_IS(GETFILEINWATCHEDDIR))
    return 0;

  // Run a script: read it in
  cIndex2 = 0;
  cIndex = MAX_MACROS + MAX_ONE_LINE_SCRIPTS + mNumTempMacros++;
  try {
    mStrCopy = "opening file";
    sFile = new CStdioFile(cReport, CFile::modeRead | CFile::shareDenyWrite);
    mStrCopy = "reading file";
    mMacros[cIndex] = "";
    while (sFile->ReadString(mItem1upper)) {
      if (!mMacros[cIndex].IsEmpty())
        mMacros[cIndex] += "\r\n";
      mMacros[cIndex] += mItem1upper;
    }
  }
  catch (CFileException * perr) {
    perr->Delete();
    cIndex2 = 1;
  }
  delete sFile;

  // Delete the file
  try {
    CFile::Remove(cReport);
  }
  catch (CFileException * pEx) {
    pEx->Delete();
    mStrCopy = "removing";
    cIndex2 = 0;
  }
  if (cIndex2)
    ABORT_LINE("Error " + mStrCopy + " " + cReport + " for line:\n\n");
  if (mMacros[cIndex].IsEmpty()) {
    mLogRpt += ", which is empty (nothing to run)";
    return 0;
  }

  // Set it up like callScript
  mCallIndex[mCallLevel++] = mCurrentIndex;
  mCurrentMacro = cIndex;
  mCallMacro[mCallLevel] = mCurrentMacro;
  mBlockDepths[mCallLevel] = -1;
  mCallFunction[mCallLevel] = NULL;
  mCurrentIndex = 0;
  mLastIndex = -1;
  mNeedClearTempMacro = mCurrentMacro;
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
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  if (mStrCopy.IsEmpty())
    ABORT_LINE("Missing directory name in statement:\n\n");
  if (CMD_IS(SETDIRECTORY)) {
    if (_chdir((LPCTSTR)mStrCopy))
      SUSPEND_NOLINE("because of failure to change directory to " + mStrCopy);
  } else {
    if (CFile::GetStatus((LPCTSTR)mStrCopy, cStatus)) {
      mWinApp->AppendToLog("Not making directory " + mStrCopy + " - it already exists");
    } else {
      if (!_mkdir((LPCTSTR)mStrCopy))
        mWinApp->AppendToLog("Created directory " + mStrCopy);
      else
        SUSPEND_NOLINE("because of failure to create directory " + mStrCopy);
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
  if (CheckConvertFilename(mStrItems, mStrLine, 1, cReport))
    return 1;
  cIndex = CFile::GetStatus((LPCTSTR)cReport, cStatus) ? 1 : 0;
  mLogRpt.Format("File %s DOES%s exist", cReport, cIndex ? "" : " NOT");
  SetReportedValues(cIndex);
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
  cIndex = mItemInt[1];
  if (cIndex < 1 || cIndex > mWinApp->mDocWnd->GetNumStores())
    ABORT_LINE("Number of file to switch to is absent or out of range in statement:"
      " \n\n");
  mWinApp->mDocWnd->SetCurrentStore(cIndex - 1);
  return 0;
}

// ReportFileNumber
int CMacCmd::ReportFileNumber(void)
{
  cIndex = mWinApp->mDocWnd->GetCurrentStore();
  if (cIndex >= 0) {
    cIndex++;
    mLogRpt.Format("Current open file number is %d", cIndex);
  } else
    mLogRpt = "There is no file open currently";
  SetReportedValues(&mStrItems[1], cIndex);
  return 0;
}

// AddToAutodoc, WriteAutodoc
int CMacCmd::AddToAutodoc(void)
{
  if (!mWinApp->mStoreMRC)
    ABORT_LINE("There is no open image file for: \n\n");
  if (mBufferManager->CheckAsyncSaving())
    ABORT_NOLINE("There was an error writing to file.\n"
    "Not adding to the autodoc because it would go into the wrong section");
  cIndex = mWinApp->mStoreMRC->GetAdocIndex();
  cIndex2 = mWinApp->mStoreMRC->getDepth();
  if (cIndex < 0)
    ABORT_LINE("There is no .mdoc file for the current image file for: \n\n");
  if (AdocGetMutexSetCurrent(cIndex) < 0)
    ABORT_LINE("Error making autodoc be the current one for: \n\n");
  if (CMD_IS(ADDTOAUTODOC)) {
    SubstituteLineStripItems(mStrLine, 2, mStrCopy);
    mParamIO->ParseString(mStrLine, mStrItems, MAX_MACRO_TOKENS, mParseQuotes);
    if (AdocSetKeyValue(
      cIndex2 ? B3DCHOICE(mWinApp->mStoreMRC->getStoreType() == STORE_TYPE_ADOC,
      ADOC_IMAGE, ADOC_ZVALUE) : ADOC_GLOBAL,
      cIndex2 ? cIndex2 - 1 : 0, (LPCTSTR)mStrItems[1], (LPCTSTR)mStrCopy)) {
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
  const char * frameMsg[] = { "There is no autodoc for frames open in:\n\n",
    "Error selecting frame .mdoc as current autodoc in:\n\n",
    "There is no current section to add to in frame .mdoc for:\n\n",
    "Error adding to frame .mdoc in:\n\n",
    "Error writing to frame .mdoc in:\n\n" };
  if (CMD_IS(ADDTOFRAMEMDOC)) {
    SubstituteLineStripItems(mStrLine, 2, mStrCopy);
    mParamIO->ParseString(mStrLine, mStrItems, MAX_MACRO_TOKENS, mParseQuotes);
    cIndex = mWinApp->mDocWnd->AddValueToFrameMdoc(mStrItems[1], mStrCopy);
  }
  else {
    cIndex = mWinApp->mDocWnd->WriteFrameMdoc();
  }
  if (cIndex > 0)
    ABORT_LINE(CString(frameMsg[cIndex - 1]));
  return 0;
}

// ReportFrameMdocOpen
int CMacCmd::ReportFrameMdocOpen(void)
{
  cIndex = mWinApp->mDocWnd->GetFrameAdocIndex() >= 0 ? 1 : 0;
  SetReportedValues(&mStrItems[1], cIndex);
  mLogRpt.Format("Autodoc for frames %s open", cIndex ? "IS" : "is NOT");
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
  SubstituteVariables(&mStrLine, 1, mStrLine);
  if (mWinApp->mDocWnd->GetFrameAdocIndex() >= 0)
    ABORT_LINE("The frame mdoc file is already open for line:\n\n");
  if (CheckConvertFilename(mStrItems, mStrLine, 1, cReport))
    return 1;
  if (mWinApp->mDocWnd->DoOpenFrameMdoc(cReport))
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
  cDoBack = CMD_IS(STARTNEXTFRAMESTACKMDOC);
  SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  mParamIO->ParseString(mStrLine, mStrItems, MAX_MACRO_TOKENS, mParseQuotes);
  if (mCamera->AddToNextFrameStackMdoc(mStrItems[1], mStrCopy, cDoBack, &cReport))
    ABORT_LINE(cReport + " in:\n\n");
  return 0;
}

// AlignWholeTSOnly
int CMacCmd::AlignWholeTSOnly(void)
{
  cIndex  = (mItemEmpty[1] || mItemInt[1] != 0) ? 1 : 0;
  if (mCamera->IsConSetSaving(&mConSets[RECORD_CONSET], RECORD_CONSET, mCamParams, false)
    && (mConSets[RECORD_CONSET].alignFrames || !cIndex) &&
    mConSets[RECORD_CONSET].useFrameAlign > 1 && mCamera->GetAlignWholeSeriesInIMOD()) {
      if (cIndex && !mWinApp->mStoreMRC)
        ABORT_LINE("There must be an output file before this command can be used:\n\n");
      if (cIndex && mWinApp->mStoreMRC->GetAdocIndex() < 0)
        ABORT_NOLINE("The output file was not opened with an associated .mdoc\r\n"
          "file, which is required to align whole tilt series in IMOD");
      SaveControlSet(RECORD_CONSET);
      mConSets[RECORD_CONSET].alignFrames = 1 - cIndex;
      mAlignWholeTSOnly = cIndex > 0;
  } else
    cIndex = 0;
  SetReportedValues(cIndex);
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
  cTruth = CMD_IS(CLOSELOGOPENNEW);
  if (mWinApp->mLogWindow) {
    cReport = mWinApp->mLogWindow->GetSaveFile();
    if (!cReport.IsEmpty())
      mWinApp->mLogWindow->DoSave();
    else if ((!cTruth || mItemEmpty[1] || !mItemInt[1]) &&
      mWinApp->mLogWindow->AskIfSave("closing and opening it again?")) {
      AbortMacro();
      return 1;
    }
    mWinApp->mLogWindow->SetUnsaved(false);
      mWinApp->mLogWindow->CloseLog();
  }
  mWinApp->AppendToLog(mWinApp->mDocWnd->DateTimeForTitle());
  if (!mItemEmpty[1] && !cTruth) {
    SubstituteLineStripItems(mStrLine, 1, cReport);
    mWinApp->mLogWindow->UpdateSaveFile(true, cReport);
  }
  return 0;
}

// SaveLog
int CMacCmd::SaveLog(void)
{
  if (!mWinApp->mLogWindow)
    ABORT_LINE("The log window must already be open for line:\n\n");
  if (mItemEmpty[2]) {
    cReport = mWinApp->mLogWindow->GetSaveFile();
    if (!cReport.IsEmpty())
      mWinApp->mLogWindow->DoSave();
    else
      mWinApp->OnFileSavelog();
  } else {
    SubstituteLineStripItems(mStrLine, 2, cReport);
    mWinApp->mLogWindow->UpdateSaveFile(true, cReport, true, mItemInt[1] != 0);
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
  cTruth = CMD_IS(REPORTPROPERTY);
  mStrCopy = cTruth ? "property" : "user setting";
  if ((!cTruth && mParamIO->MacroGetSetting(mStrItems[1], cDelX)) ||
    (cTruth && mParamIO->MacroGetProperty(mStrItems[1], cDelX)))
    ABORT_LINE(mStrItems[1] + " is not a recognized " + mStrCopy + " or cannot be "
    "accessed by script command in:\n\n");
  SetReportedValues(&mStrItems[2], cDelX);
  mLogRpt.Format("Value of %s %s is %g", (LPCTSTR)mStrCopy, (LPCTSTR)mStrItems[1], cDelX);
  return 0;
}

// SetUserSetting
int CMacCmd::SetUserSetting(void)
{
  if (mParamIO->MacroGetSetting(mStrItems[1], cDelX) ||
    mParamIO->MacroSetSetting(mStrItems[1], mItemDbl[2]))
      ABORT_LINE(mStrItems[1] + " is not a recognized setting or cannot be set by "
      "script command in:\n\n");
  mWinApp->UpdateWindowSettings();

  // See if property already saved
  cTruth = false;
  for (cIndex = 0; cIndex < (int)mSavedSettingNames.size(); cIndex++) {
    if (mStrItems[1].CompareNoCase(mSavedSettingNames[cIndex].c_str()) == 0) {
      cTruth = true;
      break;
    }
  }

  // If old value should be saved, either replace existing new value or push old and new
  if (mItemEmpty[3] || !mItemInt[3]) {
    if (cTruth) {
      mNewSettingValues[cIndex] = mItemDbl[2];
    } else {
      mSavedSettingNames.push_back(std::string((LPCTSTR)mStrItems[1]));
      mSavedSettingValues.push_back(cDelX);
      mNewSettingValues.push_back(mItemDbl[2]);
    }
  } else if (cTruth) {

    // Otherwise get rid of a saved value if new value being kept
    mSavedSettingNames.erase(mSavedSettingNames.begin() + cIndex);
    mSavedSettingValues.erase(mSavedSettingValues.begin() + cIndex);
    mNewSettingValues.erase(mNewSettingValues.begin() + cIndex);
  }
  return 0;
}

// Copy
int CMacCmd::Copy(void)
{
  if (ConvertBufferLetter(mStrItems[1], -1, true, cIndex, cReport, true))
    ABORT_LINE(cReport);
  if (ConvertBufferLetter(mStrItems[2], -1, false, cIndex2, cReport))
    ABORT_LINE(cReport);
  cImBuf = ImBufForIndex(cIndex);
  if (mBufferManager->CopyImBuf(cImBuf, &mImBufs[cIndex2]))
    SUSPEND_LINE("because of buffer copy failure in statement: \n\n");
  return 0;
}

// Show
int CMacCmd::Show(void)
{
  if (ConvertBufferLetter(mStrItems[1], -1, true, cIndex, cReport))
    ABORT_LINE(cReport);
  mWinApp->SetCurrentBuffer(cIndex);
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
  double *focTab = mScope->GetLMFocusTable();
  cIndex = mScope->GetMagIndex();
  if (cIndex <= 0 || mWinApp->GetSTEMMode())
    ABORT_NOLINE("You cannot set eucentric focus in diffraction or STEM mode");
  cDelX = focTab[cIndex * 2 + mScope->GetProbeMode()];
  if (cDelX < -900.)
    cDelX = mScope->GetStandardLMFocus(cIndex);
  if (cDelX < -900.)
    ABORT_NOLINE("There is no standard/eucentric focus defined for the current mag"
      " range");
  mScope->SetFocus(cDelX);
  if (focTab[cIndex * 2 + mScope->GetProbeMode()] < -900. && !JEOLscope &&
    cIndex >= mScope->GetLowestMModeMagInd())
    mWinApp->AppendToLog("WARNING: Setting eucentric focus using a calibrated "
      "standard focus\r\n   from the nearest mag, not the current mag");
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
  if (fabs(mItemDbl[1]) > 100.)
    ABORT_LINE("Change in target defocus too large in statement: \n\n");
  cDelX = mWinApp->mFocusManager->GetTargetDefocus();
  mWinApp->mFocusManager->SetTargetDefocus((float)(cDelX + mItemDbl[1]));
  mWinApp->mAlignFocusWindow.UpdateSettings();
  return 0;
}

// SetTargetDefocus
int CMacCmd::SetTargetDefocus(void)
{
  if (mItemDbl[1] < -200. || mItemDbl[1] > 50.)
    ABORT_LINE("Target defocus too large in statement: \n\n");
  mWinApp->mFocusManager->SetTargetDefocus(mItemFlt[1]);
  mWinApp->mAlignFocusWindow.UpdateSettings();
  return 0;
}

// ReportAutofocusOffset
int CMacCmd::ReportAutofocusOffset(void)
{
  cDelX = mWinApp->mFocusManager->GetDefocusOffset();
  mStrCopy.Format("Autofocus offset is: %.2f um", cDelX);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetReportedValues(&mStrItems[1], cDelX);
  return 0;
}

// SetAutofocusOffset
int CMacCmd::SetAutofocusOffset(void)
{
  if (mItemDbl[1] < -200. || mItemDbl[1] > 200.)
    ABORT_LINE("Autofocus offset too large in statement: \n\n");
  if (mFocusOffsetToRestore < -9000.) {
    mFocusOffsetToRestore = mWinApp->mFocusManager->GetDefocusOffset();
    mNumStatesToRestore++;
  }
  mWinApp->mFocusManager->SetDefocusOffset(mItemFlt[1]);
  return 0;
}

// SetObjFocus
int CMacCmd::SetObjFocus(void)
{
  cIndex = mItemInt[1];
  cDelX = mScope->GetDefocus();
  mScope->SetObjFocus(cIndex);
  cDelY = mScope->GetDefocus();
  cReport.Format("Defocus before = %.4f   after = %.4f", cDelX, cDelY);
  mWinApp->AppendToLog(cReport, LOG_SWALLOW_IF_CLOSED);
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
  cIndex = B3DNINT(mItemDbl[1]);
  if (!mItemEmpty[2] && mWinApp->GetSTEMMode()) {
    mScope->SetSTEMMagnification(mItemDbl[1]);
  } else {
    cI = FindIndexForMagValue(cIndex, -1, -2);
    if (!cI)
      ABORT_LINE("The value is not near enough to an existing mag in:\n\n");
    mScope->SetMagIndex(cI);
    UpdateLDAreaIfSaved();
  }
  return 0;
}

// SetMagIndex
int CMacCmd::SetMagIndex(void)
{
  cIndex = mItemInt[1];
  if (cIndex <= 0 || cIndex >= MAX_MAGS)
    ABORT_LINE("Invalid magnification index in:\n\n");
  cDelX = mCamParams->GIF ? mMagTab[cIndex].EFTEMmag : mMagTab[cIndex].mag;
  if (mCamParams->STEMcamera)
    cDelX =  mMagTab[cIndex].STEMmag;
  if (!cDelX)
    ABORT_LINE("There is a zero in the magnification table at the index given in:\n\n");
  mScope->SetMagIndex(cIndex);
  UpdateLDAreaIfSaved();
  return 0;
}

// ChangeMag, IncMagIfFoundPixel
int CMacCmd::ChangeMag(void)
{
  cIndex = mScope->GetMagIndex();
  if (!cIndex)
    SUSPEND_NOLINE("because you cannot use ChangeMag in diffraction mode");
  cIy0 = mCurrentCam;
  cIx0 = B3DNINT(mItemDbl[1]);
  if (!CMD_IS(CHANGEMAG)) {
    cIx0 = mItemDbl[1] < 0 ? - 1 : 1;
    cTruth = mProcessImage->GetFoundPixelSize(cIy0, cIndex) > 0.;
    SetReportedValues(cTruth ? 1. : 0.);
  }
  if (CMD_IS(CHANGEMAG) || cTruth) {
    cIndex2 = mWinApp->FindNextMagForCamera(cIy0, cIndex, cIx0);
    if (cIndex2 < 0)
      ABORT_LINE("Improper mag change in statement: \n\n");
    mScope->SetMagIndex(cIndex2);
  }
  UpdateLDAreaIfSaved();
  return 0;
}

// ChangeMagAndIntensity, SetMagAndIntensity
int CMacCmd::ChangeMagAndIntensity(void)
{

  // Get starting and ending mag
  cIndex = mScope->GetMagIndex();
  if (!cIndex || mScope->GetSTEMmode())
    SUSPEND_NOLINE("because you cannot use Change/SetMagAndIntensity in diffraction"
    " or STEM mode");
  if (CMD_IS(CHANGEMAGANDINTENSITY)) {
    cIndex2 = mWinApp->FindNextMagForCamera(mCurrentCam, cIndex,
      B3DNINT(mItemDbl[1]));
    if (mItemEmpty[1] || cIndex2 < 1)
      ABORT_LINE("Improper mag change in statement: \n\n");
  } else {
    cI = B3DNINT(mItemDbl[1]);
    cIndex2 = FindIndexForMagValue(cI, -1, -2);
    if (!cIndex2)
      ABORT_LINE("The value is not near enough to an existing mag in:\n\n");
  }

  // Get the intensity and change in intensity
  cDelISX = mScope->GetIntensity();
  cDelISX = mScope->GetC2Percent(mScope->FastSpotSize(), cDelISX);
  cI = mCurrentCam;
  cDelISY = pow((double)mShiftManager->GetPixelSize(cI, cIndex) /
    mShiftManager->GetPixelSize(cI, cIndex2), 2.);
  cI = mWinApp->mBeamAssessor->AssessBeamChange(cDelISY, cDelX, cDelY, -1);
  if (CheckIntensityChangeReturn(cI))
    return 1;

  // Change the mag then the intensity
  mScope->SetMagIndex(cIndex2);
  if (!cI)
    mScope->DelayedSetIntensity(cDelX, GetTickCount());
  mStrCopy.Format("%s before mag change %.3f%s, remaining factor of change "
    "needed %.3f", mScope->GetC2Name(), cDelISX, mScope->GetC2Units(), cDelY);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetReportedValues(cDelISX, cDelY);
  UpdateLDAreaIfSaved();
  return 0;
}

// SetCamLenIndex
int CMacCmd::SetCamLenIndex(void)
{
  cIndex = B3DNINT(mItemDbl[1]);
  if (mItemEmpty[1] || cIndex < 1)
    ABORT_LINE("Improper camera length index in statement: \n\n");
  if (!mScope->SetCamLenIndex(cIndex))
    ABORT_LINE("Error setting camera length index in statement: \n\n");
  UpdateLDAreaIfSaved();
  return 0;
}

// SetSpotSize
int CMacCmd::SetSpotSize(void)
{
  cIndex = B3DNINT(mItemDbl[1]);
  if (mItemEmpty[1] || cIndex < mScope->GetMinSpotSize() ||
    cIndex > mScope->GetNumSpotSizes())
    ABORT_LINE("Improper spot size in statement: \n\n");
  if (!mScope->SetSpotSize(cIndex)) {
    AbortMacro();
    return 1;
  }
  UpdateLDAreaIfSaved();
  return 0;
}

// SetProbeMode
int CMacCmd::SetProbeMode(void)
{
  if (mItem1upper.Find("MICRO") == 0 || mItem1upper == "1")
    cIndex = 1;
  else if (mItem1upper.Find("NANO") == 0 || mItem1upper == "0")
    cIndex = 0;
  else
    ABORT_LINE("Probe mode must be 0, 1, nano, or micro in statement: \n\n");
  if (!mScope->SetProbeMode(cIndex)) {
    AbortMacro();
    return 1;
  }
  UpdateLDAreaIfSaved();
  return 0;
}

// Delay
int CMacCmd::Delay(void)
{
  cDelISX = mItemDbl[1];
  if (!mStrItems[2].CompareNoCase("MSEC"))
    mSleepTime = cDelISX;
  else if (!mStrItems[2].CompareNoCase("SEC"))
    mSleepTime = 1000. * cDelISX;
  else if (!mStrItems[2].CompareNoCase("MIN"))
    mSleepTime = 60000. * cDelISX;
  else if (cDelISX > 60)
    mSleepTime = cDelISX;
  else
    mSleepTime = 1000. * cDelISX;
  mSleepStart = GetTickCount();
  if (mSleepTime > 3000)
    mWinApp->SetStatusText(SIMPLE_PANE, "WAITING FOR DELAY");
  return 0;
}

// WaitForMidnight
int CMacCmd::WaitForMidnight(void)
{
  // Get the times before and after the target time and the alternative target
  cDelX = cDelY = 5.;
  if (!mItemEmpty[1])
    cDelX = mItemDbl[1];
  if (!mItemEmpty[2])
    cDelY = mItemDbl[2];
  cIx0 = 24;
  cIx1 = 0;
  if (!mItemEmpty[3] && !mItemEmpty[4]) {
    cIx0 = mItemInt[3];
    cIx1 = mItemInt[4];
  }

  // Compute minutes into the day and the interval from now to the target
  CTime time = CTime::GetCurrentTime();
  cDelISX = time.GetHour() * 60 + time.GetMinute() + time.GetSecond() / 60.;
  cDelISY = cIx0 * 60 + cIx1 - cDelISX;

  // If wthin the window at all, set up the sleep
  if (cDelISY + cDelY > 0 && cDelISY < cDelX) {
    mSleepTime = 60000. * (cDelISY + cDelY);
    mSleepStart = GetTickCount();
    cReport.Format("Sleeping until %.1f minutes after ", cDelY);
    mStrCopy = "midnight";
    if (!mItemEmpty[4])
      mStrCopy.Format("%02d:%02d", cIx0, cIx1);
    mWinApp->AppendToLog(cReport + mStrCopy, mLogAction);
    mStrCopy.MakeUpper();
    cReport.Format(" + %.0f", cDelY);
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
  cIndex = mScope->GetScreenPos() == spDown ? 1 : 0;
  mLogRpt.Format("Screen is %s", cIndex ? "DOWN" : "UP");
  SetReportedValues(&mStrItems[1], cIndex);
  return 0;
}

// ReportScreenCurrent
int CMacCmd::ReportScreenCurrent(void)
{
  cDelX = mScope->GetScreenCurrent();
  mLogRpt.Format("Screen current is %.3f nA", cDelX);
  SetReportedValues(&mStrItems[1], cDelX);
  return 0;
}

// ImageShiftByPixels
int CMacCmd::ImageShiftByPixels(void)
{
  cBInv = mShiftManager->CameraToIS(mScope->GetMagIndex());
  cIx1 = BinDivisorI(mCamParams);
  cIndex = B3DNINT(mItemDbl[1] * cIx1);
  cIndex2 = B3DNINT(mItemDbl[2] * cIx1);

  cDelISX = cBInv.xpx * cIndex + cBInv.xpy * cIndex2;
  cDelISY = cBInv.ypx * cIndex + cBInv.ypy * cIndex2;
  if (AdjustBTApplyISSetDelay(cDelISX, cDelISY, mItemInt[4],
    !mItemEmpty[3], mItemDbl[3], cReport))
    ABORT_LINE(cReport);
  return 0;
}

// ImageShiftByUnits
int CMacCmd::ImageShiftByUnits(void)
{
  cDelISX = mItemDbl[1];
  cDelISY = mItemDbl[2];
  if (AdjustBTApplyISSetDelay(cDelISX, cDelISY, mItemInt[4],
    !mItemEmpty[3], mItemDbl[3], cReport))
    ABORT_LINE(cReport);

  // Report distance on specimen
  cAMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
  cDelX = cAMat.xpx * cDelISX + cAMat.xpy * cDelISY;
  cDelY = cAMat.ypx * cDelISX + cAMat.ypy * cDelISY;
  cSpecDist = 1000. * sqrt(cDelX * cDelX + cDelY * cDelY);
  mStrCopy.Format("%.1f nm shifted on specimen", cSpecDist);
  mWinApp->AppendToLog(mStrCopy, LOG_OPEN_IF_CLOSED);
  return 0;
}

// ImageShiftByMicrons
int CMacCmd::ImageShiftByMicrons(void)
{
  cDelX = mItemDbl[1];
  cDelY = mItemDbl[2];
  cAMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
  cBInv = mShiftManager->MatInv(cAMat);
  cDelISX = cBInv.xpx * cDelX + cBInv.xpy * cDelY;
  cDelISY = cBInv.ypx * cDelX + cBInv.ypy * cDelY;
  if (AdjustBTApplyISSetDelay(cDelISX, cDelISY, mItemInt[4],
    !mItemEmpty[3], mItemDbl[3], cReport))
    ABORT_LINE(cReport);
  return 0;
}

// ImageShiftByStageDiff
int CMacCmd::ImageShiftByStageDiff(void)
{
  mShiftManager->GetStageTiltFactors(cBacklashX, cBacklashY);
  cIndex = mScope->GetMagIndex();
  cAMat = mShiftManager->StageToCamera(mCurrentCam, cIndex);
  cBInv = MatMul(cAMat, mShiftManager->CameraToIS(cIndex));
  mShiftManager->ApplyScaleMatrix(cBInv, mItemFlt[1] * cBacklashX,
    mItemFlt[2] *cBacklashY, cDelISX, cDelISY);
  if (AdjustBTApplyISSetDelay(cDelISX, cDelISY, mItemInt[4],
    !mItemEmpty[3], mItemDbl[3], cReport))
    ABORT_LINE(cReport);
  return 0;
}

// ImageShiftToLastMultiHole
int CMacCmd::ImageShiftToLastMultiHole(void)
{
  mWinApp->mParticleTasks->GetLastHoleImageShift(cBacklashX, cBacklashY);
  mScope->IncImageShift(cBacklashX, cBacklashY);
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
  cIndex = 0;
  if (mItemInt[1])
    cIndex = -1;
  mWinApp->mShiftCalibrator->CalibrateIS(cIndex, false, true);
  return 0;
}

// ReportFocusDrift
int CMacCmd::ReportFocusDrift(void)
{
  if (mWinApp->mFocusManager->GetLastDrift(cDelX, cDelY))
    ABORT_LINE("No drift available from last autofocus for statement: \n\n");
  mStrCopy.Format("Last drift in autofocus: %.3f %.3f nm/sec", cDelX, cDelY);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetReportedValues(&mStrItems[1], cDelX, cDelY);
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
  cIndex = CString("VFTRP").Find(mStrItems[1].Left(1));
  if (cIndex < 0)
    ABORT_NOLINE("QuickFlyback must be followed by one of V, F, T, R, or P");
  if (!(mCamParams->STEMcamera && mCamParams->FEItype))
    ABORT_NOLINE("QuickFlyback can be run only if the current camera is an FEI STEM"
    " camera")
  mWinApp->mCalibTiming->CalibrateTiming(cIndex, mItemFlt[2], false);
  return 0;
}

// ReportAutoFocus
int CMacCmd::ReportAutoFocus(void)
{
  cDelX = mWinApp->mFocusManager->GetCurrentDefocus();
  cIndex = mWinApp->mFocusManager->GetLastFailed() ? - 1 : 0;
  cIndex2 = mWinApp->mFocusManager->GetLastAborted();
  if (cIndex2)
    cIndex = cIndex2;
  if (cIndex)
    mStrCopy.Format("Last autofocus FAILED with error type %d", cIndex);
  else
    mStrCopy.Format("Last defocus in autofocus: %.2f um", cDelX);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetReportedValues(&mStrItems[1], cDelX, (double)cIndex);
  return 0;
}

// ReportTargetDefocus
int CMacCmd::ReportTargetDefocus(void)
{
  cDelX = mWinApp->mFocusManager->GetTargetDefocus();
  mStrCopy.Format("Target defocus is: %.2f um", cDelX);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetReportedValues(&mStrItems[1], cDelX);
  return 0;
}

// SetBeamShift
int CMacCmd::SetBeamShift(void)
{
  cDelX = mItemDbl[1];
  cDelY = mItemDbl[2];
  if (!mScope->SetBeamShift(cDelX, cDelY)) {
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
  if (!mScope->GetBeamShift(cDelX, cDelY)) {
    AbortMacro();
    return 1;
  }
  mStrCopy.Format("Beam shift %.3f %.3f (putative microns)", cDelX, cDelY);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetReportedValues(&mStrItems[1], cDelX, cDelY);
  return 0;
}

// ReportBeamTilt
int CMacCmd::ReportBeamTilt(void)
{
  if (!mScope->GetBeamTilt(cDelX, cDelY)) {
    AbortMacro();
    return 1;
  }
  mStrCopy.Format("Beam tilt %.3f %.3f", cDelX, cDelY);
  mWinApp->AppendToLog(mStrCopy, mLogAction);
  SetReportedValues(&mStrItems[1], cDelX, cDelY);
  return 0;
}

// SetImageShift
int CMacCmd::SetImageShift(void)
{
  cDelX = mItemDbl[1];
  cDelY = mItemDbl[2];
  cTruth = mItemInt[4];
  if (cTruth)
    mScope->GetLDCenteredShift(cDelISX, cDelISY);
  if (!mScope->SetLDCenteredShift(cDelX, cDelY)) {
    AbortMacro();
    return 1;
  }
  if (AdjustBeamTiltIfSelected(cDelX - cDelISX, cDelY - cDelISY, cTruth, cReport))
    ABORT_LINE(cReport);
  if (!mItemEmpty[3] && mItemDbl[3] > 0)
    mShiftManager->SetISTimeOut(mItemFlt[3] *mShiftManager->GetLastISDelay());
  return 0;
}

// AdjustBeamTiltforIS
int CMacCmd::AdjustBeamTiltforIS(void)
{
  if (!mItemEmpty[1] && mItemEmpty[2])
    ABORT_LINE("There must be either no entries or X and Y IS entries for line:\n\n");
  if (!mItemEmpty[2]) {
    cDelISX = mItemDbl[1];
    cDelISY = mItemDbl[2];
  } else
    mScope->GetLDCenteredShift(cDelISX, cDelISY);
  if (AdjustBeamTiltIfSelected(cDelISX, cDelISY, true, cReport))
    ABORT_LINE(cReport);
  return 0;
}

// ReportImageShift
int CMacCmd::ReportImageShift(void)
{
  if (!mScope->GetLDCenteredShift(cDelX, cDelY)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Image shift %.3f %.3f IS units", cDelX, cDelY);
  cMag = mScope->GetMagIndex();
  cAMat = mShiftManager->IStoCamera(cMag);
  cDelISX = cDelISY = cStageX = cStageY = 0.;
  if (cAMat.xpx) {
    cIndex = BinDivisorI(mCamParams);
    cDelISX = -(cDelX * cAMat.xpx + cDelY * cAMat.xpy) / cIndex;
    cDelISY = -(cDelX * cAMat.ypx + cDelY * cAMat.ypy) / cIndex;
    cH1 = cos(DTOR * mScope->GetTiltAngle());
    cBInv = MatMul(cAMat,
      MatInv(mShiftManager->StageToCamera(mCurrentCam, cMag)));
    cStageX = (cBInv.xpx * cDelX + cBInv.xpy * cDelY) / (HitachiScope ? cH1 : 1.);
    cStageY = (cBInv.ypx * cDelX + cBInv.ypy * cDelY) / (HitachiScope ? 1. : cH1);
    cReport.Format("   %.1f %.1f unbinned pixels; need stage %.3f %.3f if reset", cDelISX,
                  cDelISY, cStageX, cStageY);
    mLogRpt += cReport;
  }
  SetReportedValues(&mStrItems[1], cDelX, cDelY, cDelISX, cDelISY, cStageX, cStageY);
  return 0;
}

// SetObjectiveStigmator
int CMacCmd::SetObjectiveStigmator(void)
{
  cDelX = mItemDbl[1];
  cDelY = mItemDbl[2];
  if (!mScope->SetObjectiveStigmator(cDelX, cDelY)) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// ReportXLensDeflector, SetXLensDeflector, ReportXLensFocus, SetXLensFocus
int CMacCmd::ReportXLensDeflector(void)
{
  const char * defNames[] = {"Shift", "Tilt", "Stigmator"};
  switch (mCmdIndex) {
  case CME_REPORTXLENSDEFLECTOR:
    cIndex = mScope->GetXLensDeflector(mItemInt[1], cDelX, cDelY);
    if (!cIndex) {
      mLogRpt.Format("X Lens %s is %f %f", defNames[mItemInt[1] - 1], cDelX, cDelY);
      SetReportedValues(&mStrItems[1], cDelX, cDelY);
    }
    break;
  case CME_SETXLENSDEFLECTOR:
    cIndex = mScope->SetXLensDeflector(mItemInt[1], mItemDbl[2], mItemDbl[3]);
    break;
  case CME_REPORTXLENSFOCUS:
    cIndex = mScope->GetXLensFocus(cDelX);
    if (!cIndex) {
      mLogRpt.Format("X Lens focus is %f", cDelX);
      SetReportedValues(&mStrItems[1], cDelX);
    }
    break;
  case CME_SETXLENSFOCUS:
    cIndex = mScope->SetXLensFocus(mItemDbl[1]);
    break;
  }
  if (cIndex == 1) {
    ABORT_LINE("Scope is not initialized for:\n\n");
  } else if (cIndex == 2) {
    ABORT_LINE("Plugin is missing needed function for:\n\n");
  } else if (cIndex == 3) {
    ABORT_LINE("Deflector number must be between 1 and 3 in:\n\n");
  } else if (cIndex == 5) {
    ABORT_LINE("There is no connection to adatl COM object for:\n\n");
  } else if (cIndex > 5) {
    ABORT_LINE("X Mode is not available for:\n\n");
  }
  return 0;
}

// ReportSpecimenShift
int CMacCmd::ReportSpecimenShift(void)
{
  if (!mScope->GetLDCenteredShift(cDelISX, cDelISY)) {
    AbortMacro();
    return 1;
  }
  cAMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
  if (cAMat.xpx) {
    cDelX = cAMat.xpx * cDelISX + cAMat.xpy * cDelISY;
    cDelY = cAMat.ypx * cDelISX + cAMat.ypy * cDelISY;
    mLogRpt.Format("Image shift is %.3f  %.3f in specimen coordinates",  cDelX, cDelY);
    SetReportedValues(&mStrItems[1], cDelX, cDelY);
  } else {
    mWinApp->AppendToLog("There is no calibration for converting image shift to "
      "specimen coordinates", mLogAction);
  }
  return 0;
}

// ReportObjectiveStigmator
int CMacCmd::ReportObjectiveStigmator(void)
{
  if (!mScope->GetObjectiveStigmator(cDelX, cDelY)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Objective stigmator is %.5f %.5f", cDelX, cDelY);
  SetReportedValues(&mStrItems[1], cDelX, cDelY);
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
  cDelISY = 0.;
  if (CMD_IS(REPORTSHIFTDIFFFROM)) {
    cDelISY = mItemDbl[1];
    if (mItemEmpty[1] || cDelISY <= 0)
      ABORT_LINE("Improper or missing comparison value in statement: \n\n");
  }
  if (mImBufs->mImage) {
    float cShiftX, cShiftY;
    mImBufs->mImage->getShifts(cShiftX, cShiftY);
    cShiftX *= mImBufs->mBinning;
    cShiftY *= -mImBufs->mBinning;
    mAccumShiftX += cShiftX;
    mAccumShiftY += cShiftY;
    cIndex = mScope->GetMagIndex();
    cIndex2 = BinDivisorI(mCamParams);
    if (cDelISY) {
      cDelX = mShiftManager->GetPixelSize(mCurrentCam, cIndex);
      cDelY = cDelX * sqrt(cShiftX * cShiftX + cShiftY * cShiftY);
      cDelISX = 100. * (cDelY / cDelISY - 1.);
      mAccumDiff += (float)(cDelY - cDelISY);
      cDelISY = cDelX * sqrt(mAccumShiftX * mAccumShiftX + mAccumShiftY * mAccumShiftY);
      mStrCopy.Format("%6.1f %% diff, cumulative diff %6.2f  um, total distance %.1f",
        cDelISX, mAccumDiff, cDelISY);
      SetReportedValues(&mStrItems[2], cDelISX, mAccumDiff, cDelISY);
    } else {
      cBInv = mShiftManager->CameraToIS(cIndex);
      cAMat = mShiftManager->IStoSpecimen(cIndex);

      // Convert to image shift units, then determine distance on specimen
      // implied by each axis of image shift separately
      cDelISX = cBInv.xpx * cShiftX + cBInv.xpy * cShiftY;
      cDelISY = cBInv.ypx * cShiftX + cBInv.ypy * cShiftY;
      if (CMD_IS(REPORTISFORBUFFERSHIFT)) {
        mStrCopy.Format("Buffer shift corresponds to %.3f %.3f IS units", -cDelISX,
          - cDelISY);
        SetReportedValues(&mStrItems[1], -cDelISX, -cDelISY);
      } else {
        cH1 = 1000. * (cDelISX * cAMat.xpx + cDelISY * cAMat.xpy);
        cV1 = 1000. * (cDelISX * cAMat.ypx + cDelISY * cAMat.ypy);
        cDelX = 1000. * sqrt(pow(cDelISX * cAMat.xpx, 2) + pow(cDelISX * cAMat.ypx, 2));
        cDelY = 1000. * sqrt(pow(cDelISY * cAMat.xpy, 2) + pow(cDelISY * cAMat.ypy, 2));
        mStrCopy.Format("%6.1f %6.1f unbinned pixels; %6.1f %6.1f nm along two shift "
          "axes; %6.1f %6.1f nm on specimen axes",
          cShiftX / cIndex2, cShiftY / cIndex2, cDelX, cDelY, cH1, cV1);
        SetReportedValues(&mStrItems[1], cShiftX, cShiftY, cDelX, cDelY, cH1, cV1);
      }
    }
    mWinApp->AppendToLog(mStrCopy, mLogAction);
  }
  return 0;
}

// ReportAccumShift
int CMacCmd::ReportAccumShift(void)
{
  cIndex2 = BinDivisorI(mCamParams);
  mLogRpt.Format("%8.1f %8.1f cumulative pixels", mAccumShiftX / cIndex2,
    mAccumShiftY / cIndex2);
  SetReportedValues(&mStrItems[1], mAccumShiftX, mAccumShiftY);
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
  mShiftManager->GetLastAlignTrims(cIx0, cIx1, cIy0, cIy1);
  mLogRpt.Format("Total border trimmed in last autoalign in X & Y for A: %d %d   "
    "Reference: %d %d", cIx0, cIx1, cIy0, cIy1);
  SetReportedValues(&mStrItems[1], cIx0, cIx1, cIy0, cIy1);
  return 0;
}

// CameraToISMatrix,  ISToCameraMatrix, CameraToStageMatrix, StageToCameraMatrix,
// CameraToSpecimenMatrix, SpecimenToCameraMatrix, ISToSpecimenMatrix,
// SpecimenToISMatrix, ISToStageMatrix, StageToISMatrix,
// StageToSpecimenMatrix, SpecimenToStageMatrix
int CMacCmd::CameraToISMatrix(void)
{
  cIndex = mItemInt[1];
  if (cIndex <= 0)
    cIndex = mScope->GetMagIndex();
  if (!cIndex)
    ABORT_LINE("There are no scale matrices for diffraction mode for line:\n\n");
  if (cIndex >= MAX_MAGS)
    ABORT_LINE("The mag index is out of range in line:\n\n");
  cTruth = false;
  switch (mCmdIndex) {

  case CME_CAMERATOISMATRIX:
    cTruth = true;
  case CME_ISTOCAMERAMATRIX:
    cAMat = mShiftManager->IStoGivenCamera(cIndex, mCurrentCam);
    break;
  case CME_CAMERATOSTAGEMATRIX:
    cTruth = true;
  case CME_STAGETOCAMERAMATRIX:
    cAMat = mShiftManager->StageToCamera(mCurrentCam, cIndex);
    break;

  case CME_SPECIMENTOCAMERAMATRIX:
    cTruth = true;
  case CME_CAMERATOSPECIMENMATRIX:
    cAMat = mShiftManager->CameraToSpecimen(cIndex);
    break;

  case CME_SPECIMENTOISMATRIX:
    cTruth = true;
  case CME_ISTOSPECIMENMATRIX:
    cAMat = cAMat = mShiftManager->IStoSpecimen(cIndex, mCurrentCam);
    break;
  case CME_STAGETOISMATRIX:
    cTruth = true;
  case CME_ISTOSTAGEMATRIX:
    cAMat = mShiftManager->StageToCamera(mCurrentCam, cIndex);
    if (cAMat.xpx) {
      cBInv = mShiftManager->StageToCamera(mCurrentCam, cIndex);
      if (cBInv.xpx)
        cAMat = MatMul(cAMat, MatInv(cBInv));
      else
        cAMat.xpx = 0.;
    }
    break;
  case CME_STAGETOSPECIMENMATRIX:
    cTruth = true;
  case CME_SPECIMENTOSTAGEMATRIX:
    cAMat = mShiftManager->SpecimenToStage(1., 1.);
    break;
  }
  if (cTruth && cAMat.xpx)
    cAMat = MatInv(cAMat);
  cReport = cmdList[mCmdIndex].mixedCase;
  cReport.Replace("To", " to ");
  cReport.Replace("Matrix", " matrix");
  if (cAMat.xpx)
    mLogRpt.Format("%s at mag index %d is %.5g  %.5g  %.5g  %.5g", (LPCTSTR)cReport,
      cIndex, cAMat.xpx, cAMat.xpy, cAMat.ypx, cAMat.ypy);
  else
    mLogRpt.Format("%s is not available for mag index %d", (LPCTSTR)cReport, cIndex);
  SetReportedValues(&mStrItems[2], cAMat.xpx, cAMat.xpy, cAMat.ypx, cAMat.ypy);
  return 0;
}

// ReportClock
int CMacCmd::ReportClock(void)
{
  cDelX = 0.001 * GetTickCount() - 0.001 * mStartClock;
  mLogRpt.Format("%.2f seconds elapsed time", cDelX);
  SetReportedValues(&mStrItems[1], cDelX);
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
  cIndex = mWinApp->MinuteTimeStamp();
  if (!mItemEmpty[1] && SetVariable(mStrItems[1], (double)cIndex, VARTYPE_PERSIST, -1,
    false, &cReport))
      ABORT_LINE(cReport + "\n(This command assigns to a persistent variable):\n\n");
  mLogRpt.Format("Absolute minute time = %d", cIndex);
  SetReportedValues(cIndex);
  return 0;
}

// SetCustomTime, ReportCustomInterval
int CMacCmd::SetCustomTime(void)
{
  std::string sstr = (LPCTSTR)mItem1upper;
  std::map < std::string, int > ::iterator custIter;
  cIndex = mWinApp->MinuteTimeStamp();
  if (!mItemEmpty[2])
    cIndex = mItemInt[2];
  cIndex2 = (int)mCustomTimeMap.count(sstr);
  if (cIndex2)
    custIter = mCustomTimeMap.find(sstr);
  if (CMD_IS(SETCUSTOMTIME)) {

    // Insert does not replace a value!  You have to get the iterator and assign it
    if (cIndex2)
      custIter->second = cIndex;
    else
      mCustomTimeMap.insert(std::pair < std::string, int > (sstr, cIndex));
    mWinApp->mDocWnd->SetShortTermNotSaved();
  } else {
    if (cIndex2) {
      cIndex -= custIter->second;
      mLogRpt.Format("%d minutes elapsed since custom time %s set", cIndex,
         (LPCTSTR)mStrItems[1]);
    } else {
      cIndex = 2 * MAX_CUSTOM_INTERVAL;
      mLogRpt = "Custom time " + mStrItems[1] + " has not been set";
    }
    SetReportedValues(cIndex);
  }
  return 0;
}

// ReportTickTime
int CMacCmd::ReportTickTime(void)
{
  cDelISX = SEMTickInterval(mWinApp->ProgramStartTime()) / 1000.;
  if (!mItemEmpty[1] && SetVariable(mStrItems[1], cDelISX, VARTYPE_PERSIST, -1,
    false, &cReport))
      ABORT_LINE(cReport + "\n(This command assigns to a persistent variable):\n\n");
  mLogRpt.Format("Tick time from program start = %.3f", cDelISX);
  SetReportedValues(cDelISX);
  return 0;
}

// ElapsedTickTime
int CMacCmd::ElapsedTickTime(void)
{
  cDelISY = SEMTickInterval(mWinApp->ProgramStartTime());
  cDelISX = SEMTickInterval(cDelISY, mItemDbl[1] * 1000.) / 1000.;
  mLogRpt.Format("Elapsed tick time = %.3f", cDelISX);
  SetReportedValues(&mStrItems[2], cDelISX);
  return 0;
}

// ReportDateTime
int CMacCmd::ReportDateTime(void)
{
  CTime ctDateTime = CTime::GetCurrentTime();
  cIndex = 10000 * ctDateTime.GetYear() + 100 * ctDateTime.GetMonth() +
    ctDateTime.GetDay();
  cIndex2 = 100 * ctDateTime.GetHour() + ctDateTime.GetMinute();
  mLogRpt.Format("%d  %04d", cIndex, cIndex2);
  SetReportedValues(&mStrItems[1], cIndex, cIndex2);
  return 0;
}

// MoveStage, MoveStageTo, MoveStageWithSpeed, TestRelaxingStage, StageShiftByPixels, 
// StageToLastMultiHole
int CMacCmd::MoveStage(void)
{
    cSmi.z = 0.;
    cSmi.alpha = 0.;
    cSmi.axisBits = 0;
    cSmi.useSpeed = false;
    cSmi.backX = cSmi.backY = cSmi.relaxX = cSmi.relaxY = 0.;
    cTruth = CMD_IS(TESTRELAXINGSTAGE);

    // If stage not ready, back up and try again, otherwise do action
    if (mScope->StageBusy() > 0)
      mLastIndex = mCurrentIndex;
    else {
      if (CMD_IS(STAGESHIFTBYPIXELS)) {
        cH1 = DTOR * mScope->GetTiltAngle();
        cAMat = mShiftManager->StageToCamera(mCurrentCam,
          mScope->GetMagIndex());
        if (!cAMat.xpx)
          ABORT_LINE("There is no stage to camera calibration available for line:\n\n");
        cBInv = MatInv(cAMat);
        cStageX = cBInv.xpx * mItemDbl[1] + cBInv.xpy * mItemDbl[2];
        cStageY = (cBInv.ypx * mItemDbl[1] + cBInv.ypy * mItemDbl[2]) / cos(cH1);
        cStageZ = 0.;
      } else if (CMD_IS(STAGETOLASTMULTIHOLE)) {
        mWinApp->mParticleTasks->GetLastHoleStagePos(cStageX, cStageY);
        if (cStageX < EXTRA_VALUE_TEST)
          ABORT_LINE("The multiple Record routine has not been run for line:\n\n");
        cStageZ = 0.;
      } else {

        if (mItemEmpty[2])
          ABORT_LINE("Stage movement command does not have at least 2 numbers: \n\n");
        cStageX = mItemDbl[1];
        cStageY = mItemDbl[2];
        if (CMD_IS(MOVESTAGEWITHSPEED)) {
          cStageZ = 0.;
          cSmi.useSpeed = true;
          cSmi.speed = mItemDbl[3];
          if (cSmi.speed <= 0.)
            ABORT_LINE("Speed entry must be positive in line:/n/n");
        } else {
          cStageZ = (mItemEmpty[3] || cTruth) ? 0. : mItemDbl[3];
        }
      }
      if (CMD_IS(MOVESTAGE) || CMD_IS(STAGESHIFTBYPIXELS) || CMD_IS(MOVESTAGEWITHSPEED)
        || cTruth) {
        if (!mScope->GetStagePosition(cSmi.x, cSmi.y, cSmi.z))
          SUSPEND_NOLINE("because of failure to get stage position");
          //CString report;
        //report.Format("Start at %.2f %.2f %.2f", smi.x, smi.y, smi.z);
        //mWinApp->AppendToLog(report, LOG_IF_ADMIN_OPEN_IF_CLOSED);

        // For each of X, Y, Z, set axis bit if nonzero;
        cSmi.x += cStageX;
        if (cStageX != 0.)
          cSmi.axisBits |= axisX;
        cSmi.y += cStageY;
        if (cStageY != 0.)
          cSmi.axisBits |= axisY;
        cSmi.z += cStageZ;
        if (cStageZ != 0.)
          cSmi.axisBits |= axisZ;
        if (cTruth) {
          cBacklashX = mItemEmpty[3] ? mWinApp->mMontageController->GetStageBacklash() :
            mItemFlt[3];
        cBacklashY = mItemEmpty[4] ? mScope->GetStageRelaxation() : mItemFlt[4];
        if (cBacklashY <= 0)
          cBacklashY = 0.025f;

        // Set relaxation in the direction of backlash
        if (cStageX) {
          cSmi.backX = (cStageX > 0. ? cBacklashX : -cBacklashX);
          cSmi.relaxX = (cSmi.backX > 0. ? cBacklashY : -cBacklashY);
        }
        if (cStageY) {
          cSmi.backY = (cStageY > 0. ? cBacklashX : -cBacklashX);
          cSmi.relaxY = (cSmi.backY > 0. ? cBacklashY : -cBacklashY);
        }
        }
      } else {

        // Absolute move: set to values; set Z axis bit if entered
        cSmi.x = cStageX;
        cSmi.y = cStageY;
        cSmi.z = cStageZ;

        cSmi.axisBits |= (axisX | axisY);
        if (!mItemEmpty[3] && !CMD_IS(STAGETOLASTMULTIHOLE))
          cSmi.axisBits |= axisZ;
      }

      // Start the movement
      if (cSmi.axisBits) {
        if (!mScope->MoveStage(cSmi, cTruth && cBacklashX != 0., cSmi.useSpeed, false,
          cTruth))
          SUSPEND_NOLINE("because of failure to start stage movement");
        mMovedStage = true;
      }
    }
  return 0;
}

// RelaxStage
int CMacCmd::RelaxStage(void)
{
  cDelX = mItemEmpty[1] ? mScope->GetStageRelaxation() : mItemDbl[1];
  if (!mScope->GetStagePosition(cSmi.x, cSmi.y, cSmi.z))
    SUSPEND_NOLINE("because of failure to get stage position");
  if (mScope->GetValidXYbacklash(cSmi.x, cSmi.y, cBacklashX, cBacklashY)) {
    mScope->CopyBacklashValid();

    // Move in direction of the backlash, which is opposite to direction of stage move
    cSmi.x += (cBacklashX > 0. ? cDelX : -cDelX);
    cSmi.y += (cBacklashY > 0. ? cDelX : -cDelX);
    cSmi.z = 0.;
    cSmi.alpha = 0.;
    cSmi.axisBits = axisXY;
    mScope->MoveStage(cSmi);
      mMovedStage = true;
  } else {
    mWinApp->AppendToLog("Stage is not in known backlash state so RelaxStage cannot "
      "be done", mLogAction);
  }
  return 0;
}

// BackgroundTilt
int CMacCmd::BackgroundTilt(void)
{
  if (mScope->StageBusy() > 0)
    mLastIndex = mCurrentIndex;
  else {
    cSmi.alpha = mItemDbl[1];
    cDelX = 1.;
    cSmi.useSpeed = false;
    if (!mItemEmpty[2]) {
      cSmi.speed = mItemDbl[2];
      if (cSmi.speed <= 0.)
        ABORT_LINE("Speed entry must be positive in line:/n/n");
      cSmi.useSpeed = true;
    }
    cSmi.axisBits = axisA;
    if (!mScope->MoveStage(cSmi, false, cSmi.useSpeed, true)) {
      AbortMacro();
      return 1;
    }
  }
  return 0;
}

// ReportStageXYZ
int CMacCmd::ReportStageXYZ(void)
{
  mScope->GetStagePosition(cStageX, cStageY, cStageZ);
  mLogRpt.Format("Stage %.2f %.2f %.2f", cStageX, cStageY, cStageZ);
  SetReportedValues(&mStrItems[1], cStageX, cStageY, cStageZ);
  return 0;
}

// ReportTiltAngle
int CMacCmd::ReportTiltAngle(void)
{
  cDelX = mScope->GetTiltAngle();
  mLogRpt.Format("%.2f degrees", cDelX);
  SetReportedValues(&mStrItems[1], cDelX);
  return 0;
}

// ReportStageBusy
int CMacCmd::ReportStageBusy(void)
{
  cIndex = mScope->StageBusy();
  mLogRpt.Format("Stage is %s", cIndex > 0 ? "BUSY" : "NOT busy");
  SetReportedValues(&mStrItems[1], cIndex > 0 ? 1 : 0);
  return 0;
}

// ReportStageBAxis
int CMacCmd::ReportStageBAxis(void)
{
  cDelX = mScope->GetStageBAxis();
  mLogRpt.Format("B axis = %.2f degrees", cDelX);
  SetReportedValues(&mStrItems[1], cDelX);
  return 0;
}

// ReportMag
int CMacCmd::ReportMag(void)
{
  cIndex2 = mScope->GetMagIndex();

  // This is not right if the screen is down and FEI is not in EFTEM
  cIndex = MagOrEFTEMmag(mWinApp->GetEFTEMMode(), cIndex2, mScope->GetSTEMmode());
  mLogRpt.Format("Mag is %d%s", cIndex,
    cIndex2 < mScope->GetLowestNonLMmag() ? "   (Low mag)":"");
  SetReportedValues(&mStrItems[1], (double)cIndex,
    cIndex2 < mScope->GetLowestNonLMmag() ? 1. : 0.);
  return 0;
}

// ReportMagIndex
int CMacCmd::ReportMagIndex(void)
{
  cIndex = mScope->GetMagIndex();
  mLogRpt.Format("Mag index is %d%s", cIndex,
    cIndex < mScope->GetLowestNonLMmag() ? "   (Low mag)":"");
  SetReportedValues(&mStrItems[1], (double)cIndex,
    cIndex < mScope->GetLowestNonLMmag() ? 1. : 0.);
  return 0;
}

// ReportCameraLength
int CMacCmd::ReportCameraLength(void)
{
  cDelX = 0;
  if (!mScope->GetMagIndex())
    cDelX = mScope->GetLastCameraLength();
  mLogRpt.Format("%s %g%s", cDelX ? "Camera length is " : "Not in diffraction mode - (",
    cDelX, cDelX ? " m" : ")");
  SetReportedValues(&mStrItems[1],  cDelX);
  return 0;
}

// ReportDefocus
int CMacCmd::ReportDefocus(void)
{
  cDelX = mScope->GetDefocus();
  mLogRpt.Format("Defocus = %.3f um", cDelX);
  SetReportedValues(&mStrItems[1], cDelX);
  return 0;
}

// ReportFocus, ReportAbsoluteFocus
int CMacCmd::ReportFocus(void)
{
  cDelX = mScope->GetFocus();
  mLogRpt.Format("Absolute focus = %.5f", cDelX);
  SetReportedValues(&mStrItems[1], cDelX);
  return 0;
}

// ReportPercentC2
int CMacCmd::ReportPercentC2(void)
{
  cDelY = mScope->GetIntensity();
  cDelX = mScope->GetC2Percent(mScope->FastSpotSize(), cDelY);
  mLogRpt.Format("%s = %.3f%s  -  %.5f", mScope->GetC2Name(), cDelX, mScope->GetC2Units(),
    cDelY);
  SetReportedValues(&mStrItems[1], cDelX, cDelY);
  return 0;
}

// ReportCrossoverPercentC2
int CMacCmd::ReportCrossoverPercentC2(void)
{
  cIndex = mScope->GetSpotSize();
  cDelY = mScope->GetCrossover(cIndex); // probe mode not required, uses current mode
  cDelX = mScope->GetC2Percent(cIndex, cDelY);
  mLogRpt.Format("Crossover %s at current conditions = %.3f%s  -  %.5f",
    mScope->GetC2Name(), cDelX, mScope->GetC2Units(), cDelY);
  SetReportedValues(&mStrItems[1], cDelX, cDelY);
  return 0;
}

// ReportIlluminatedArea
int CMacCmd::ReportIlluminatedArea(void)
{
  cDelX = mScope->GetIlluminatedArea();
  mLogRpt.Format("IlluminatedArea %f", cDelX);
  SetReportedValues(&mStrItems[1], cDelX);
  return 0;
}

// ReportImageDistanceOffset
int CMacCmd::ReportImageDistanceOffset(void)
{
  cDelX = mScope->GetImageDistanceOffset();
  mLogRpt.Format("Image distance offset %f", cDelX);
  SetReportedValues(&mStrItems[1], cDelX);
  return 0;
}

// ReportAlpha
int CMacCmd::ReportAlpha(void)
{
  cIndex = mScope->GetAlpha() + 1;
  mLogRpt.Format("Alpha = %d", cIndex);
  SetReportedValues(&mStrItems[1], (double)cIndex);
  return 0;
}

// ReportSpotSize
int CMacCmd::ReportSpotSize(void)
{
  cIndex = mScope->GetSpotSize();
  mLogRpt.Format("Spot size is %d", cIndex);
  SetReportedValues(&mStrItems[1], (double)cIndex);
  return 0;
}

// ReportProbeMode
int CMacCmd::ReportProbeMode(void)
{
  cIndex = mScope->ReadProbeMode();
  mLogRpt.Format("Probe mode is %s", cIndex ? "microprobe" : "nanoprobe");
  SetReportedValues(&mStrItems[1], (double)cIndex);
  return 0;
}

// ReportEnergyFilter
int CMacCmd::ReportEnergyFilter(void)
{
  if (!mWinApp->ScopeHasFilter())
    ABORT_NOLINE("You cannot use ReportEnergyFilter; there is no filter");
  if (mScope->GetHasOmegaFilter())
    mScope->updateEFTEMSpectroscopy(cTruth);
  else if (mCamera->CheckFilterSettings())
    ABORT_NOLINE("An error occurred checking filter settings");
  cDelX = mFiltParam->zeroLoss ? 0. : mFiltParam->energyLoss;
  mLogRpt.Format("Filter slit width %.1f, energy loss %.1f, slit %s",
    mFiltParam->slitWidth, cDelX, mFiltParam->slitIn ? "IN" : "OUT");
  SetReportedValues(&mStrItems[1], mFiltParam->slitWidth, cDelX,
    mFiltParam->slitIn ? 1. : 0.);
  return 0;
}

// ReportColumnMode
int CMacCmd::ReportColumnMode(void)
{
  if (!mScope->GetColumnMode(cIndex, cIndex2)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Column mode %d (%X), submode %d (%X)", cIndex, cIndex, cIndex2,cIndex2);
  SetReportedValues(&mStrItems[1], (double)cIndex, (double)cIndex2);
  return 0;
}

// ReportLens
int CMacCmd::ReportLens(void)
{
  mParamIO->StripItems(mStrLine, 1, cReport);
  if (!mScope->GetLensByName(cReport, cDelX)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Lens %s = %f", (LPCTSTR)cReport, cDelX);
  SetReportedValues(cDelX);
  return 0;
}

// ReportCoil
int CMacCmd::ReportCoil(void)
{
  if (!mScope->GetDeflectorByName(mStrItems[1], cDelX, cDelY)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Coil %s = %f  %f", (LPCTSTR)mStrItems[1], cDelX, cDelY);
  SetReportedValues(&mStrItems[2], cDelX, cDelY);
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
  if (!mScope->GetLensFLCStatus(mItemInt[1], cIndex, cDelX))
    ABORT_LINE("Error trying to run:\n\n");
  mLogRpt.Format("Lens %d, FLC %s  value  %f", mItemInt[1], cIndex ? "ON" : "OFF", cDelX);
  SetReportedValues(&mStrItems[2], cIndex, cDelX);
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

// SetCameraPLAOffset
int CMacCmd::SetCameraPLAOffset(void)
{
  if (!mImBufs[0].mImage || !mImBufs[1].mImage)
    ABORT_NOLINE("There must be images in both buffers A and B to refine detector "
      "offset");
  if (!mScope->GetUsePLforIS() || !JEOLscope)
    ABORT_NOLINE("SetCameraPLAOffset can be used only on a JEOL scope using"
      " PLA for image shift");
  mScope->GetImageShift(cDelISX, cDelISY);
  mScope->GetDetectorOffsets(cFloatX, cFloatY);
  cShiftX = cFloatX + (float)(cDelISX - mImBufs[1].mISX);
  cShiftY = cFloatY + (float)(cDelISY - mImBufs[1].mISY);
  PrintfToLog("Camera offset changed from %.3f  %.3f to %.3f %.3f", cFloatX, cFloatY, 
    cShiftX, cShiftY);
  mCamera->SetCameraISOffset(mWinApp->GetCurrentCamera(), cShiftX, cShiftY);
  mScope->SetDetectorOffsets(cShiftX, cShiftY);
  mScope->SetImageShift(cDelISX, cDelISY);
  return 0;
}

// RemoveAperture, ReInsertAperture
int CMacCmd::RemoveAperture(void)
{
  cIndex = mItemInt[1];
  if (mStrItems[1] == "CL")
    cIndex = 1;
  if (mStrItems[1] == "OL")
    cIndex = 2;
  if (CMD_IS(REINSERTAPERTURE))
    cIndex2 = mScope->ReInsertAperture(cIndex);
  else
    cIndex2 = mScope->RemoveAperture(cIndex);
  if (cIndex2)
    ABORT_LINE("Script aborted due to error starting aperture movement in:\n\n");
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
  cIndex = mScope->GetCurrentPhasePlatePos();
  if (cIndex < 0)
    ABORT_LINE("Script aborted due to error in:\n\n");
  mLogRpt.Format("Current phase plate position is %d", cIndex + 1);
  SetReportedValues(&mStrItems[1], cIndex + 1.);
  return 0;
}

// ReportMeanCounts
int CMacCmd::ReportMeanCounts(void)
{
  if (ConvertBufferLetter(mStrItems[1], 0, true, cIndex, cReport))
    ABORT_LINE(cReport);
  cImBuf = &mImBufs[cIndex];
  if (mItemEmpty[2] || mItemInt[2] < 2) {
    cDelX = mProcessImage->WholeImageMean(cImBuf);
    if (mStrItems[2] == "1" && cImBuf->mBinning && cImBuf->mExposure > 0.) {
      cDelX /= cImBuf->mBinning * cImBuf->mBinning * cImBuf->mExposure *
        mWinApp->GetGainFactor(cImBuf->mCamera, cImBuf->mBinning);
      mLogRpt.Format("Mean of buffer %s = %.2f unbinned counts/sec", mStrItems[1], cDelX);
    } else
      mLogRpt.Format("Mean of buffer %s = %.1f", mStrItems[1], cDelX);
    SetReportedValues(cDelX);
  } else {
    cImage = mImBufs[cIndex].mImage;
    cImage->getSize(cSizeX, cSizeY);
    ProcMinMaxMeanSD(cImage->getData(), cImage->getType(), cSizeX, cSizeY, 0,
      cSizeX - 1, 0, cSizeY - 1, &cBmean, &cBmin, &cBmax, &cBSD);
    mLogRpt.Format("Buffer %s: mean = %.1f  sd = %.2f  min = %.1f  max = %.1f",
      mStrItems[1], cBmean, cBSD, cBmin, cBmax);
    SetReportedValues(cBmean, cBSD, cBmin, cBmax);
  }
  return 0;
}

// ReportFileZsize
int CMacCmd::ReportFileZsize(void)
{
  if (!mWinApp->mStoreMRC)
    SUSPEND_LINE("because there is no open image file to report for line: \n\n");
  if (mBufferManager->CheckAsyncSaving())
    SUSPEND_NOLINE("because of file write error");
  cIndex = mWinApp->mStoreMRC->getDepth();
  mLogRpt.Format("Z size of file = %d", cIndex);
  SetReportedValues(&mStrItems[1], (double)cIndex);
  return 0;
}

// SubareaMean
int CMacCmd::SubareaMean(void)
{
  cIx0 = mItemInt[1];
  cIx1 = mItemInt[2];
  cIy0 = mItemInt[3];
  cIy1 = mItemInt[4];
  if (mImBufs->mImage)
    mImBufs->mImage->getSize(cSizeX, cSizeY);
  if (!mImBufs->mImage || mItemEmpty[4] || cIx0 < 0 || cIx1 >= cSizeX || cIx1 < cIx0
    || cIy0 < 0 || cIy1 >= cSizeY || cIy1 < cIy0)
      ABORT_LINE("Not enough coordinates, coordinates out of bounds, or no image "
        "in A in statement: \n\n");
  cDelX = ProcImageMean(mImBufs->mImage->getData(), mImBufs->mImage->getType(),
    cSizeX, cSizeY, cIx0, cIx1, cIy0, cIy1);
  mLogRpt.Format("Mean of subarea (%d, %d) to (%d, %d) = %.2f", cIx0, cIy0, cIx1, cIy1,
    cDelX);
  SetReportedValues(&mStrItems[5], cDelX);
  return 0;
}

// CircularSubareaMean
int CMacCmd::CircularSubareaMean(void)
{
  cIx0 = mItemInt[1];
  cIy0 = mItemInt[2];
  cI = mItemInt[3];
  if (mImBufs->mImage)
    mImBufs->mImage->getSize(cSizeX, cSizeY);
  if (!mImBufs->mImage || mItemEmpty[3] || cIx0 - cI < 0 || cIy0 - cI < 0 ||
    cIx0 + cI >= cSizeX || cIy0 + cI >= cSizeY)
    ABORT_LINE("Not enough coordinates, coordinates out of bounds, or no image "
      "in A in statement: \n\n");
  cDelX = ProcImageMeanCircle(mImBufs->mImage->getData(), mImBufs->mImage->getType(),
    cSizeX, cSizeY, cIx0, cIy0, cI);
  mLogRpt.Format("Mean of subarea with radius %d centered around (%d, %d) = %.2f", cI,
    cIx0, cIy0, cDelX);
  SetReportedValues(&mStrItems[4], cDelX);
  return 0;
}

// ElectronStats, RawElectronStats
int CMacCmd::ElectronStats(void)
{
  if (ConvertBufferLetter(mStrItems[1], 0, true, cIndex, cReport))
    ABORT_LINE(cReport);
  cDelX = mImBufs[cIndex].mBinning;
  if (mImBufs[cIndex].mCamera < 0 || !cDelX || !mImBufs[cIndex].mExposure)
    ABORT_LINE("Image buffer does not have enough information for dose statistics in:"
    "\r\n\r\n");
  cCpe = mProcessImage->CountsPerElectronForImBuf(&mImBufs[cIndex]);
  if (!cCpe)
    ABORT_LINE("Camera does not have a CountsPerElectron property for:\r\n\r\n");
  cImage = mImBufs[cIndex].mImage;
  cImage->getSize(cSizeX, cSizeY);
  ProcMinMaxMeanSD(cImage->getData(), cImage->getType(), cSizeX, cSizeY, 0,
    cSizeX - 1, 0, cSizeY - 1, &cBmean, &cBmin, &cBmax, &cBSD);
  mCamParams = mWinApp->GetCamParams() + mImBufs[cIndex].mCamera;
  if (CamHasDoubledBinnings(mCamParams))
    cDelX /= 2;
  cBmin /= cCpe;
  cBmax /= cCpe;
  cBmean /= cCpe;
  cBSD /= cCpe;
  cBacklashX = (float)(cBmean / (mImBufs[cIndex].mExposure * cDelX * cDelX));
  cBacklashY = cBacklashX;
  if (CMD_IS(ELECTRONSTATS)) {
    if (mImBufs[cIndex].mK2ReadMode > 0)
      cBacklashY = mProcessImage->LinearizedDoseRate(mImBufs[cIndex].mCamera,
        cBacklashX);
    if (mImBufs[cIndex].mDoseRatePerUBPix > 0.) {
      SEMTrace('1', "Dose rate computed from mean %.3f  returned from DM %.3f", 
        cBacklashY, mImBufs[cIndex].mDoseRatePerUBPix);
      cBacklashY = mImBufs[cIndex].mDoseRatePerUBPix;
    }
  }
  cShiftX = cBacklashY / cBacklashX;
  mLogRpt.Format("Min = %.3f  max = %.3f  mean = %.3f  SD = %.3f electrons/pixel; "
    "dose rate = %.3f e/unbinned pixel/sec", cBmin * cShiftX, cBmax * cShiftX,
    cBmean *cShiftX, cBSD * cShiftX, cBacklashY);
  SetReportedValues(&mStrItems[2], cBmin * cShiftX, cBmax * cShiftX, cBmean * cShiftX,
    cBSD *cShiftX, cBacklashY);
  return 0;
}

// CropImage, CropCenterToSize
int CMacCmd::CropImage(void)
{
  if (ConvertBufferLetter(mStrItems[1], -1, true, cIndex, cReport, true))
    ABORT_LINE(cReport);
  cImBuf = ImBufForIndex(cIndex);
  cImBuf->mImage->getSize(cSizeX, cSizeY);
  cImBuf->mImage->getShifts(cBacklashX, cBacklashY);
  cTruth = CMD_IS(CROPIMAGE);
  if (cTruth) {
    cIx0 = mItemInt[2];
    cIx1 = mItemInt[3];
    cIy0 = mItemInt[4];
    cIy1 = mItemInt[5];

    // Test for whether centered:if not, it needs to be marked as processed
    cTruth = B3DABS(cIx0 + cIx1 + 1 - cSizeX) > 1 || B3DABS(cIy0 + cIy1 + 1 - cSizeY) > 1;
  } else {
    if (mItemInt[2] > cSizeX || mItemInt[3] > cSizeY)
      ABORT_LINE("Image is already smaller than size requested in:\n\n");
    if (mItemInt[2] <= 0 || mItemInt[3] <= 0)
      ABORT_LINE("Size to crop to must be positive in:\n\n");
    cIx0 = (cSizeX - mItemInt[2]) / 2;
    cIy0 = (cSizeY - mItemInt[3]) / 2;
    cIx1 = cIx0 + mItemInt[2] - 1;
    cIy1 = cIy0 + mItemInt[3] - 1;
  }
  cIx0 = mProcessImage->CropImage(cImBuf, cIy0, cIx0, cIy1, cIx1);
  if (cIx0) {
    cReport.Format("Error # %d attempting to crop image in buffer %c in statement: \n\n"
      , cIx0, mStrItems[1].GetAt(0));
    ABORT_LINE(cReport);
  }

  // Mark as cropped if centered and unshifted: this allows autoalign to apply image
  // shift on scope
  mImBufs->mCaptured = (cTruth || cBacklashX || cBacklashY) ? BUFFER_PROCESSED :
    BUFFER_CROPPED;
  return 0;
}

// ReduceImage
int CMacCmd::ReduceImage(void)
{
  if (ConvertBufferLetter(mStrItems[1], -1, true, cIndex, cReport, true))
    ABORT_LINE(cReport);
  cIx0 = mProcessImage->ReduceImage(ImBufForIndex(cIndex), mItemFlt[2], &cReport);
  if (cIx0) {
    cReport += " in statement:\n\n";
    ABORT_LINE(cReport);
  }
  return 0;
}

// FFT
int CMacCmd::FFT(void)
{
  if (ConvertBufferLetter(mStrItems[1], -1, true, cIndex, cReport))
    ABORT_LINE(cReport);
  cIndex2 = 1;
  if (!mItemEmpty[2])
    cIndex2 = mItemInt[2];
  if (cIndex2 < 1 || cIndex2 > 8)
    ABORT_LINE("Binning must be between 1 and 8 in line:\n\n");
  mProcessImage->GetFFT(&mImBufs[cIndex], cIndex2, BUFFER_FFT);
  return 0;
}

// FilterImage
int CMacCmd::FilterImage(void)
{
  if (ConvertBufferLetter(mStrItems[1], -1, true, cIndex, cReport, true) || 
    ConvertBufferLetter(mStrItems[6], 0, false, cIndex2, cReport))
    ABORT_LINE(cReport);
  if (mProcessImage->FilterImage(ImBufForIndex(cIndex), cIndex2, mItemFlt[2], mItemFlt[5],
    mItemFlt[3], mItemFlt[4]))
    ABORT_LINE("Failed to filter image for line:\n\n");
  return 0;
}

// AddImages, SubtractImages, MultiplyImages, DivideImages
int CMacCmd::CombineImages(void)
{
  if (ConvertBufferLetter(mStrItems[1], -1, true, cIndex, cReport) ||
    ConvertBufferLetter(mStrItems[2], -1, true, cIndex2, cReport) ||
    ConvertBufferLetter(mStrItems[3], 0, false, cIx1, cReport))
    ABORT_LINE(cReport);
  cIx0 = PROC_ADD_IMAGES;
  if (CMD_IS(SUBTRACTIMAGES))
    cIx0 = PROC_SUBTRACT_IMAGES;
  if (CMD_IS(MULTIPLYIMAGES))
    cIx0 = PROC_MULTIPLY_IMAGES;
  if (CMD_IS(DIVIDEIMAGES))
    cIx0 = PROC_DIVIDE_IMAGES;
  if (mProcessImage->CombineImages(cIndex, cIndex2, cIx1, cIx0))
    ABORT_LINE("Failed to combine images for line:\n\n");
  return 0;
}

// ScaleImage
int CMacCmd::ScaleImage(void)
{
  if (ConvertBufferLetter(mStrItems[1], -1, true, cIndex, cReport, true) ||
    ConvertBufferLetter(mStrItems[4], 0, false, cIndex2, cReport))
    ABORT_LINE(cReport);
  if (mProcessImage->ScaleImage(ImBufForIndex(cIndex), cIndex2, mItemFlt[2], mItemFlt[3],
    !mItemEmpty[5] && mItemInt[5]))
    ABORT_LINE("Failed to scale image for line:\n\n");
  return 0;
}

// CtfFind
int CMacCmd::CtfFind(void)
{
  if (ConvertBufferLetter(mStrItems[1], -1, true, cIndex, cReport))
    ABORT_LINE(cReport);
  CtffindParams param;
  float resultsArray[7];
  if (mProcessImage->InitializeCtffindParams(&mImBufs[cIndex], param))
    ABORT_LINE("Error initializing Ctffind parameters for line:\n\n");
  if (mItemDbl[2] > 0. || mItemDbl[3] > 0. || mItemDbl[2] < -100 || mItemDbl[3] < -100)
    ABORT_LINE("Minimum and maximum defocus must be negative and in microns");
  if (mItemDbl[2] < mItemDbl[3]) {
    cDelX = mItemDbl[2];
    mItemDbl[2] = mItemDbl[3];
    mItemDbl[3] = cDelX;
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
    cDelX = mItemDbl[6] * DTOR;
    if (cDelX == 0)
      cDelX = mProcessImage->GetPlatePhase();
    param.minimum_additional_phase_shift = param.maximum_additional_phase_shift =
      (float)(cDelX);
    param.find_additional_phase_shift = true;
    if (!mItemEmpty[7] && (mItemDbl[6] != 0 || mItemDbl[7] != 0)) {
      cDelY = mItemDbl[7] * DTOR;
      if (cDelY < cDelX)
        ABORT_LINE("The maximum phase shift is less than the minimum in line:\n\n");
      if (cDelY - cDelX > 125 * DTOR)
        ABORT_LINE("The range of phase shift to search is more than 125 degrees in "
        "line:\n\n");
      param.maximum_additional_phase_shift = (float)cDelY;
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
  if (mProcessImage->RunCtffind(&mImBufs[cIndex], param, resultsArray))
    ABORT_LINE("Ctffind fitting returned an error for line:\n\n");
  SetReportedValues(-(resultsArray[0] + resultsArray[1]) / 20000.,
    (resultsArray[0] - resultsArray[1]) / 10000., resultsArray[2],
    resultsArray[3] / DTOR, resultsArray[4], resultsArray[5]);
  return 0;
}

// ImageProperties
int CMacCmd::ImageProperties(void)
{
  if (ConvertBufferLetter(mStrItems[1], 0, true, cIndex, cReport, true))
    ABORT_LINE(cReport);
  cImBuf = ImBufForIndex(cIndex);
  cImBuf->mImage->getSize(cSizeX, cSizeY);
  cDelX = 1000. * mWinApp->mShiftManager->GetPixelSize(&mImBufs[cIndex]);
  mLogRpt.Format("Image size %d by %d, binning %s, exposure %.4f",
    cSizeX, cSizeY, (LPCTSTR)cImBuf->BinningText(),
    cImBuf->mExposure);
  if (cDelX) {
    mStrCopy.Format(", pixel size " + mWinApp->PixelFormat((float)cDelX), (float)cDelX);
    mLogRpt += mStrCopy;
  }
  if (cImBuf->mSecNumber < 0) {
    cDelY = -1;
    mStrCopy = ", read in";
  } else {
    cDelY = cImBuf->mConSetUsed;
    mStrCopy.Format(", %s parameters", mModeNames[cImBuf->mConSetUsed]);
  }
  mLogRpt += mStrCopy;
  if (cImBuf->mMapID) {
    mStrCopy.Format(", mapID %d", cImBuf->mMapID);
    mLogRpt += mStrCopy;
  }
  SetReportedValues(&mStrItems[2], (double)cSizeX, (double)cSizeY,
    cImBuf->mBinning / (double)B3DMAX(1, cImBuf->mDivideBinToShow),
    (double)cImBuf->mExposure, cDelX, cDelY);
  return 0;
}

// ImageLowDoseSet
int CMacCmd::ImageLowDoseSet(void)
{
  if (ConvertBufferLetter(mStrItems[1], 0, true, cIndex, cReport))
    ABORT_LINE(cReport);
  cIndex2 = mImBufs[cIndex].mConSetUsed;

  if (cIndex2 == MONTAGE_CONSET)
    cIndex2 = RECORD_CONSET;
  if (mImBufs[cIndex].mLowDoseArea && cIndex2 >= 0) {
    mLogRpt = "Image is from " + mModeNames[cIndex2] + " in Low Dose";
  } else {
    if (cIndex2 == TRACK_CONSET) {

      // Try to match View vs R by mag first
      cMag = mImBufs[cIndex].mMagInd;
      if (mLdParam[VIEW_CONSET].magIndex == cMag) {
        cIndex2 = VIEW_CONSET;
      } else if (mLdParam[RECORD_CONSET].magIndex == cMag) {

        // Then decide between preview and record based on exposure
        cDelX = mImBufs[cIndex].mExposure;
        if (fabs(cDelX - mConSets[RECORD_CONSET].exposure) <
          fabs(cDelX - mConSets[PREVIEW_CONSET].exposure))
          cIndex2 = RECORD_CONSET;
        else
          cIndex2 = PREVIEW_CONSET;
      }
      mLogRpt = "Image matches one from " + mModeNames[cIndex2] + " in Low Dose";
    } else if (cIndex2 >= 0) {
      mLogRpt = "Image is from " + mModeNames[cIndex2] + " parameters, not in Low Dose";
    }
  }
  if (cIndex2 > SEARCH_CONSET || cIndex2 < 0) {
    cIndex2 = -1;
    mLogRpt = "Image properties do not match any Low Dose area well enough";
  }
  SetReportedValues(&mStrItems[2], (double)cIndex2,
    mImBufs[cIndex].mLowDoseArea ? 1. : 0.);
  return 0;
}

// MeasureBeamSize
int CMacCmd::MeasureBeamSize(void)
{
  if (ConvertBufferLetter(mStrItems[1], 0, true, cIndex, cReport))
    ABORT_LINE(cReport);
  cIx0 = mProcessImage->FindBeamCenter(&mImBufs[cIndex], cBacklashX, cBacklashY,
    cCpe, cBmin, cBmax, cBmean, cBSD, cIndex2, cIx1, cShiftX, cShiftY, cFitErr);
  if (cIx0)
    ABORT_LINE("No beam edges were detected in image for line:\n\n");
  cBmean = mWinApp->mShiftManager->GetPixelSize(&mImBufs[cIndex]);
  if (!cBmean)
    ABORT_LINE("No pixel size is available for the image for line:\n\n");
  cCpe *= 2.f * cIndex2 * cBmean;
  mLogRpt.Format("Beam diameter measured to be %.3f um from %d quadrants, fit error %.3f"
    , cCpe, cIx1, cFitErr);
  SetReportedValues(cCpe, cIx1, cFitErr);
  return 0;
}

// QuadrantMeans
int CMacCmd::QuadrantMeans(void)
{
  cDelY = 0.1;
  cDelX = 0.1;
  cIndex = 2;
  if (!mItemEmpty[1])
    cIndex = mItemInt[1];
  if (!mItemEmpty[2])
    cDelX = mItemDbl[2];
  if (!mItemEmpty[3])
    cDelY = mItemDbl[3];
  cImage = mImBufs->mImage;
  if (cImage)
    cImage->getSize(cSizeX, cSizeY);
  if (!cImage || cIndex > B3DMIN(cSizeX, cSizeY) / 4 || cIndex < 0 || cDelX <= 0.
    || cDelY < 0. || cDelX > 0.5 || cDelY > 0.4)
      ABORT_LINE("Parameter out of bounds for image, or no image in A in statement:"
      "\n\n");
  cIx0 = B3DMAX((int)(cDelY * cSizeX / 2), 1);   // Trim length in X
  cIy0 = B3DMAX((int)(cDelY * cSizeY / 2), 1);   // Trim length in Y
  cIx1 = B3DMAX((int)(cDelX * cSizeX / 2), 1);   // Width in X
  cIy1 = B3DMAX((int)(cDelX * cSizeY / 2), 1);   // Width in Y
  cH4 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
    cSizeX / 2 + cIndex, cSizeX - 1 - cIx0, cSizeY / 2 + cIndex, 
    cSizeY / 2 + cIndex + cIy1 - 1);
  cV4 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
    cSizeX / 2 + cIndex, cSizeX / 2 + cIndex + cIx1 - 1, cSizeY / 2 + cIndex, 
    cSizeY - 1 - cIy0);
  cV3 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
    cSizeX / 2 - cIndex - cIx1, cSizeX / 2 - cIndex - 1, cSizeY / 2 + cIndex, 
    cSizeY - 1 - cIy0);
  cH3 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
    cIx0, cSizeX / 2 - cIndex - 1, cSizeY / 2 + cIndex, cSizeY / 2 + cIndex + cIy1 - 1);
  cH2 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
    cIx0, cSizeX / 2 - cIndex - 1, cSizeY / 2 - cIndex - cIy1, cSizeY / 2 - cIndex - 1);
  cV2 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
    cSizeX / 2 - cIndex - cIx1, cSizeX / 2 - cIndex - 1, cIy0, cSizeY / 2 - cIndex - 1);
  cV1 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
    cSizeX / 2 + cIndex, cSizeX / 2 + cIndex + cIx1 - 1, cIy0, cSizeY / 2 - cIndex - 1);
  cH1 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
    cSizeX / 2 + cIndex, cSizeX - 1 - cIx0, cSizeY / 2 - cIndex - cIy1, 
    cSizeY / 2 - cIndex - 1);
  cReport.Format("h1, v1, v2, h2, h3, v3, v4, h4:  %.2f   %.2f   %.2f   %.2f   %.2f   "
    "%.2f   %.2f   %.2f", cH1, cV1, cV2, cH2, cH3, cV3, cV4, cH4);
  mWinApp->AppendToLog(cReport, LOG_OPEN_IF_CLOSED);
  return 0;
}

// CheckForBadStripe
int CMacCmd::CheckForBadStripe(void)
{
  if (ConvertBufferLetter(mStrItems[1], 0, true, cIndex, cReport))
    ABORT_LINE(cReport);
  cIx0 = mItemInt[2];
  if (mItemEmpty[2]) {
    if (mImBufs[cIndex].mCamera < 0)
      ABORT_LINE("The image has no camera defined and requires an entry for horizontal"
        " versus vertical analysis in:\n\n");
    cIx0 = (mWinApp->GetCamParams() + mImBufs[cIndex].mCamera)->rotationFlip % 2;
  }
  cIndex2 = mProcessImage->CheckForBadStripe(&mImBufs[cIndex], cIx0, cIx1);
  if (!cIndex2)
    mLogRpt = "No bad stripes detected";
  else
    mLogRpt.Format("Bad stripes detected with %d sharp transitions; %d near expected"
      " boundaries", cIndex2, cIx1);
  SetReportedValues(cIndex2, cIx1);
  return 0;
}

// CircleFromPoints
int CMacCmd::CircleFromPoints(void)
{
  float radius, xcen, ycen;
  if (mItemEmpty[6])
    ABORT_LINE("Need three sets of X, Y coordinates on line:\n" + mStrLine);
  cH1 = mItemDbl[1];
  cV1 = mItemDbl[2];
  cH2 = mItemDbl[3];
  cV2 = mItemDbl[4];
  cH3 = mItemDbl[5];
  cV3 = mItemDbl[6];
  if (circleThrough3Pts((float)cH1, (float)cV1, (float)cH2, (float)cV2, (float)cH3,
    (float)cV3, &radius, &xcen, &ycen)) {
      mWinApp->AppendToLog("The three points are too close to being on a straight line"
        , LOG_OPEN_IF_CLOSED);
  } else {
    cReport.Format("Circle center = %.2f  %.2f  radius = %.2f", xcen, ycen, radius);
    mWinApp->AppendToLog(cReport, LOG_OPEN_IF_CLOSED);
    SetReportedValues(&mStrItems[7], xcen, ycen, radius);
  }
  return 0;
}

// FindPixelSize
int CMacCmd::FindPixelSize(void)
{
  mProcessImage->FindPixelSize(0., 0., 0., 0.);
  return 0;
}

// Echo, EchoNoVarSub
int CMacCmd::Echo(void)
{
  if (CMD_IS(ECHO))
    SubstituteVariables(&mStrLine, 1, mStrLine);
  mParamIO->StripItems(mStrLine, 1, cReport, true);
  cReport.Replace("\n", "  ");
  mWinApp->AppendToLog(cReport, LOG_OPEN_IF_CLOSED);
  return 0;
}

// EchoEval, EchoReplaceLine, EchoNoLineEnd
int CMacCmd::EchoEval(void)
{
  cReport = "";
  for (cIndex = 1; cIndex < MAX_MACRO_TOKENS && !mItemEmpty[cIndex]; cIndex++) {
    if (cIndex > 1)
      cReport += " ";
    cReport += mStrItems[cIndex];
  }
  cReport.Replace("\n", "  ");
  cIndex2 = 0;
  if (CMD_IS(ECHOREPLACELINE))
    cIndex2 = 3;
  else if (CMD_IS(ECHONOLINEEND))
    cIndex2 = 1;
  mWinApp->AppendToLog(cReport, LOG_OPEN_IF_CLOSED, cIndex2);
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
  mWinApp->AppendToLog(mWinApp->GetStartupMessage(mItemInt[1] != 0));
  return 0;
}

// IsVersionAtLeast, SkipIfVersionLessThan
int CMacCmd::IsVersionAtLeast(void)
{
  cIndex2 = mItemEmpty[2] ? 0 : mItemInt[2];
  cIx0 = mWinApp->GetIntegerVersion();
  cIx1 = mWinApp->GetBuildDayStamp();
  cTruth = mItemInt[1] <= cIx0 && cIndex2 <= cIx1;
  if (CMD_IS(ISVERSIONATLEAST)) {
    SetReportedValues(cTruth ? 1. : 0.);
    mLogRpt.Format("Program version is %d date %d, %s than %d %s", cIx0, cIx1,
      cTruth ? "later" : "earlier", mItemInt[1], (LPCTSTR)mStrItems[2]);
  } else if (!cTruth && mCurrentIndex < mMacro->GetLength())
    GetNextLine(mMacro, mCurrentIndex, mStrLine);
  return 0;
}

int CMacCmd::IsFFTWindowOpen(void)
{
  cIndex = mWinApp->mFFTView ? 1 : 0;
  mLogRpt.Format("FFT window %s open", cIndex ? "IS" : "is NOT");
  SetReportedValues(&mStrItems[1], cIndex);
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
  if (envar)
    SetOneReportedValue(&mStrItems[2], CString(envar), 2);
  return 0;
}

// ReportSettingsFile
int CMacCmd::ReportSettingsFile(void)
{
  cReport = mWinApp->mDocWnd->GetCurrentSettingsPath();
  mWinApp->AppendToLog("Current settings file: " + cReport);
  SetOneReportedValue(&mStrItems[1], cReport, 1);
  cReport = "None";
  if (mWinApp->mDocWnd->GetReadScriptPack()) {
    cReport = mWinApp->mDocWnd->GetCurScriptPackPath();
    mWinApp->AppendToLog("Current script package: " + cReport);
  }
  SetOneReportedValue(&mStrItems[1], cReport, 2);
  return 0;
}

// ListAllCalibrations
int CMacCmd::ListAllCalibrations(void)
{
  mWinApp->mMenuTargets.DoListISVectors(true);
  mWinApp->mMenuTargets.DoListStageCals();
  mShiftManager->ListBeamShiftCals();
  mWinApp->mBeamAssessor->ListIntensityCalibrations();
  mWinApp->mBeamAssessor->ListSpotCalibrations();

  return 0;
}

// Pause, YesNoBox, PauseIfFailed, AbortIfFailed
int CMacCmd::Pause(void)
{
  cDoPause = CMD_IS(PAUSEIFFAILED);
  cDoAbort = CMD_IS(ABORTIFFAILED);
  if (!(cDoPause || cDoAbort) || !mLastTestResult) {
    SubstituteLineStripItems(mStrLine, 1, cReport);
    if (cDoPause || cDoAbort)
      mWinApp->AppendToLog(cReport);
    cDoPause = cDoPause || CMD_IS(PAUSE);
    if (cDoPause) {
      cReport += "\n\nDo you want to proceed with the script?";
      cIndex = AfxMessageBox(cReport, cDoAbort ? MB_EXCLAME : MB_QUESTION);
    } else
      cIndex = SEMMessageBox(cReport, cDoAbort ? MB_EXCLAME : MB_QUESTION);
    if ((cDoPause && cIndex == IDNO) || cDoAbort) {
      SuspendMacro(cDoAbort);
      return 1;
    } else
      SetReportedValues(cIndex == IDYES ? 1. : 0.);
  }
  return 0;
}

// OKBox
int CMacCmd::OKBox(void)
{
  SubstituteLineStripItems(mStrLine, 1, cReport);
  AfxMessageBox(cReport, MB_OK | MB_ICONINFORMATION);
  return 0;
}

// EnterOneNumber, EnterDefaultedNumber
int CMacCmd::EnterOneNumber(void)
{
  cBacklashX = 0.;
  cIndex = 1;
  cIndex2 = 3;
  if (CMD_IS(ENTERDEFAULTEDNUMBER)) {

    // Here, enter the value and the number of digits, or < 0 to get an integer entry
    cBacklashX = mItemFlt[1];
    cIndex = 3;
    cIndex2 = mItemInt[2];
    if (cIndex2 < 0)
      cIx0 = mItemInt[1];
  }
  SubstituteLineStripItems(mStrLine, cIndex, mStrCopy);
  if (cIndex2 >= 0) {
    cTruth = KGetOneFloat(mStrCopy, cBacklashX, cIndex2);
  } else {
    cTruth = KGetOneInt(mStrCopy, cIx0);
    cBacklashX = (float)cIx0;
  }
  if (!cTruth)
    SUSPEND_NOLINE("because no number was entered");
  mLogRpt.Format("%s: user entered  %g", (LPCTSTR)mStrCopy, cBacklashX);
  SetReportedValues(cBacklashX);
  return 0;
}

// EnterString
int CMacCmd::EnterString(void)
{
  mStrCopy = "Enter a text string:";
  cReport = "";
  SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  if (!KGetOneString(mStrCopy, cReport))
    SUSPEND_NOLINE("because no string was entered");
  if (SetVariable(mItem1upper, cReport, VARTYPE_REGULAR, -1, false))
    ABORT_NOLINE("Error setting variable " + mStrItems[1] + " with string " + cReport);
  return 0;
}

// ThreeChoiceBox
int CMacCmd::ThreeChoiceBox(void)
{
  Variable *vars[5];
  CString buttons[3];
  cTruth = mItemEmpty[5];
  for (cI = 0; cI < (cTruth ? 4 : 5); cI++) {
    cReport = mStrItems[cI + 1];
    cReport.MakeUpper();
    vars[cI] = LookupVariable(cReport, cIndex2);
    if (!vars[cI])
      ABORT_LINE("The variable " + mStrItems[cI + 1] + " is not defined in line:\n\n");
    if (vars[cI]->rowsFor2d)
      ABORT_LINE("The variable " + mStrItems[cI + 1] + " is a 2D array which is not "
        "allowed for line:\n\n");
  }
  cReport = "";
  for (cI = 0; cI < (cTruth ? 3 : 4); cI++) {
    if (!cReport.IsEmpty())
      cReport += "\n\n";
    cReport += vars[cI]->value;
  }

  cValPtr = &vars[cTruth ? 3 : 4]->value;
  FindValueAtIndex(*cValPtr, 1, cIx0, cIx1);
  buttons[0] = cValPtr->Mid(cIx0, cIx1 - cIx0);
  FindValueAtIndex(*cValPtr, 2, cIx0, cIx1);
  buttons[1] = cValPtr->Mid(cIx0, cIx1 - cIx0);

  if (!cTruth) {
    FindValueAtIndex(*cValPtr, 3, cIx0, cIx1);
    buttons[2] = cValPtr->Mid(cIx0, cIx1 - cIx0);
  }
  cI = SEMThreeChoiceBox(cReport, buttons[0], buttons[1], buttons[2],
    (cTruth ? MB_YESNO : MB_YESNOCANCEL) | MB_ICONQUESTION, 0, false);
  if (cI == IDYES)
    cIndex = 1;
  else if (cI == IDNO)
    cIndex = 2;
  else
    cIndex = 3;
  SetReportedValues(cIndex);
  mLogRpt = "\"" + buttons[cIndex - 1] + "\" was chosen";
  return 0;
}

// CompareNoCase, CompareStrings
int CMacCmd::CompareNoCase(void)
{
  cVar = LookupVariable(mItem1upper, cIndex2);
  if (!cVar)
    ABORT_LINE("The variable " + mStrItems[1] + " is not defined in line:\n\n");
  SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  if (CMD_IS(COMPARESTRINGS))
    cIndex = cVar->value.Compare(mStrCopy);
  else
    cIndex = cVar->value.CompareNoCase(mStrCopy);
  mLogRpt.Format("The strings %s equal", cIndex ? "are NOT" : "ARE");
  SetReportedValues(cIndex);
  return 0;
}

// StripEndingDigits
int CMacCmd::StripEndingDigits(void)
{
  cVar = LookupVariable(mItem1upper, cIndex2);
  if (!cVar)
    ABORT_LINE("The variable " + mStrItems[1] + " is not defined in line:\n\n");
  cReport = cVar->value;
  for (cIndex = cReport.GetLength() - 1; cIndex > 0; cIndex--)
    if ((int)cReport.GetAt(cIndex) < '0' || (int)cReport.GetAt(cIndex) > '9')
      break;
  mStrCopy = cReport.Right(cReport.GetLength() - (cIndex + 1));
  cReport = cReport.Left(cIndex + 1);
  mItem1upper = mStrItems[2];
  mItem1upper.MakeUpper();
  if (SetVariable(mItem1upper, cReport, VARTYPE_REGULAR, -1, false))
    ABORT_LINE("Error setting variable " + mStrItems[2] + " with string " + cReport +
    " in:\n\n");
  SetReportedValues(atoi((LPCTSTR)mStrCopy));
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
  SubstituteLineStripItems(mStrLine, 1, cReport);
  if (CMD_IS(SENDEMAIL)) {
    mWinApp->mMailer->SendMail(mMailSubject, cReport);
  } else {
    if (!mWinApp->mMailer->GetInitialized() || mWinApp->mMailer->GetSendTo().IsEmpty())
      ABORT_LINE("Mail server not initialized or email address not defined; cannot "
      "do:\n");
    mEmailOnError = cReport;
  }
  return 0;
}

// ClearAlignment
int CMacCmd::ClearAlignment(void)
{
  cDoShift = (mItemEmpty[1] || !mItemInt[1]) && !mScope->GetNoScope();
  mShiftManager->SetAlignShifts(0., 0., false, mImBufs, cDoShift);
  return 0;
}

// ResetImageShift
int CMacCmd::ResetImageShift(void)
{
  cTruth = mShiftManager->GetBacklashMouseAndISR();
  cBacklashX = 0.;
  if (mItemInt[1] > 0) {
    mShiftManager->SetBacklashMouseAndISR(true);
    if (mItemInt[1] > 1)
      cBacklashX = mItemEmpty[2] ? mScope->GetStageRelaxation() : mItemFlt[2];
  }
  cIndex = mShiftManager->ResetImageShift(true, false, 10000, cBacklashX);
  mShiftManager->SetBacklashMouseAndISR(cTruth);
  if (cIndex) {
    mCurrentIndex = mLastIndex;
    SuspendMacro();
    return 1;
  }
  return 0;
}

// ResetShiftIfAbove
int CMacCmd::ResetShiftIfAbove(void)
{
  if (mItemEmpty[1])
    ABORT_LINE("ResetShiftIfAbove must be followed by a number in: \n\n");
  mScope->GetLDCenteredShift(cDelISX, cDelISY);
  cAMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
  cDelX = cAMat.xpx * cDelISX + cAMat.xpy * cDelISY;
  cDelY = cAMat.ypx * cDelISX + cAMat.ypy * cDelISY;
  cSpecDist = sqrt(cDelX * cDelX + cDelY * cDelY);
  if (cSpecDist > mItemDbl[1])
    mWinApp->mComplexTasks->ResetShiftRealign();
  return 0;
}

// Eucentricity
int CMacCmd::Eucentricity(void)
{
  cIndex = FIND_EUCENTRICITY_FINE;
  if (!mItemEmpty[1])
    cIndex = mItemInt[1];
  if ((cIndex & (FIND_EUCENTRICITY_FINE | FIND_EUCENTRICITY_COARSE)) == 0) {
    cReport.Format("Error in script: value on Eucentricity statement \r\n"
      "should be %d for coarse, %d for fine, or %d for both",
      FIND_EUCENTRICITY_COARSE, FIND_EUCENTRICITY_FINE,
      FIND_EUCENTRICITY_FINE | FIND_EUCENTRICITY_COARSE);
    ABORT_LINE(cReport);
  }
  mWinApp->mComplexTasks->FindEucentricity(cIndex);
  return 0;
}

// ReportLastAxisOffset
int CMacCmd::ReportLastAxisOffset(void)
{
  cDelX = mWinApp->mComplexTasks->GetLastAxisOffset();
  if (cDelX < -900)
    ABORT_NOLINE("There is no last axis offset; fine eucentricity has not been run");
  mLogRpt.Format("Lateral axis offset in last run of Fine Eucentricity was %.2f", cDelX);
  SetReportedValues(&mStrItems[1], cDelX);
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

// WalkUpTo
int CMacCmd::WalkUpTo(void)
{
  if (mItemEmpty[1])
    ABORT_LINE("WalkUpTo must be followed by a target angle in: \n\n");
  cDelISX = mItemDbl[1];
  if (cDelISX < -80. || cDelISX > 80.)
    ABORT_LINE("Target angle is too high in: \n\n");
  mWinApp->mComplexTasks->WalkUp((float)cDelISX, -1, 0);
  return 0;
}

// ReverseTilt
int CMacCmd::ReverseTilt(void)
{
  cIndex = (mScope->GetReversalTilt() > mScope->GetTiltAngle()) ? 1 : -1;
  if (!mItemEmpty[1]) {
    cIndex = mItemInt[1];
    if (cIndex > 0)
      cIndex = 1;
    else if (cIndex < 0)
      cIndex = -1;
    else
      ABORT_NOLINE("Error in script: ReverseTilt should not be followed by 0");
  }
  mWinApp->mComplexTasks->ReverseTilt(cIndex);
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
  SetReportedValues(&mStrItems[1], mWinApp->mParticleTasks->GetWDLastDriftRate(),
    mWinApp->mParticleTasks->GetWDLastFailed());
  return 0;
}

// BacklashAdjust
int CMacCmd::BacklashAdjust(void)
{
  mWinApp->mMontageController->GetColumnBacklash(cBacklashX, cBacklashY);
  mWinApp->mComplexTasks->BacklashAdjustStagePos(cBacklashX, cBacklashY, false, false);
  return 0;
}

// CenterBeamFromImage
int CMacCmd::CenterBeamFromImage(void)
{
  cTruth = mItemInt[1] != 0;
  cDelISX = !mItemEmpty[2] ? mItemDbl[2] : 0.;
  cIndex = mProcessImage->CenterBeamFromActiveImage(0., 0., cTruth, cDelISX);
  if (cIndex > 0 && cIndex <= 3)
    ABORT_LINE("Script aborted centering beam because of no image,\n"
    "unusable image type, or failure to get memory");
  SetReportedValues(cIndex);
  return 0;
}

// AutoCenterBeam
int CMacCmd::AutoCenterBeam(void)
{
  cDelISX = mItemEmpty[1] ? 0. : mItemDbl[1];
  if (mWinApp->mMultiTSTasks->AutocenterBeam((float)cDelISX)) {
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
  cIndex2 = mWinApp->LowDoseMode() ? 3 : -1;
  if (CMD_IS(SETINTENSITYFORMEAN)) {
    if (!mImBufs->mImage || mImBufs->IsProcessed() ||
      (mWinApp->LowDoseMode() &&
      mCamera->ConSetToLDArea(mImBufs->mConSetUsed) != 3))
      ABORT_LINE("Script aborted setting intensity because there\n"
        " is no image or a processed image in Buffer A\n"
        "(or, if in Low Dose mode, because the image in\n"
        " Buffer A is not from the Record area)");
    cDelISX = mItemDbl[1] /
      mProcessImage->EquivalentRecordMean(0);
  } else if (CMD_IS(SETINTENSITYBYLASTTILT)) {
    cDelISX = mIntensityFactor;
  } else {
    cDelISX = mItemDbl[1];
    cIndex2 = mScope->GetLowDoseArea();
  }

  cIndex = mWinApp->mBeamAssessor->ChangeBeamStrength(cDelISX, cIndex2);
  if (CheckIntensityChangeReturn(cIndex))
    return 1;
  UpdateLDAreaIfSaved();
  return 0;
}

// SetDoseRate
int CMacCmd::SetDoseRate(void)
{
  if (mItemDbl[1] <= 0)
    ABORT_LINE("Dose rate must be positive for line:\n\n");
  if (!mImBufs->mImage)
    ABORT_LINE("There must be an image in buffer A for line:\n\n");
  cIndex = mProcessImage->DoSetIntensity(true, mItemFlt[1]);
  if (cIndex < 0) {
    AbortMacro();
    return 1;
  }
  if (CheckIntensityChangeReturn(cIndex))
    return 1;
  UpdateLDAreaIfSaved();
  return 0;
}

// SetPercentC2, IncPercentC2
int CMacCmd::SetPercentC2(void)
{

  // The entered number is always in terms of % C2 or illuminated area, so for
  // incremental, first comparable value and add to get the absolute to set
  cDelISX = mItemDbl[1];
  if (mItemEmpty[1])
    ABORT_LINE("Script aborted because no C2 percent was entered in:\n\n");
  cIndex = mScope->FastSpotSize();
  if (CMD_IS(INCPERCENTC2))
    cDelISX += mScope->GetC2Percent(cIndex, mScope->GetIntensity());

  // Then convert to an intensity as appropriate for scope
  if (mScope->GetUseIllumAreaForC2()) {
    cDelISY = mScope->IllumAreaToIntensity(cDelISX / 100.);
  } else {
    cDelISY = (0.01 * cDelISX - mScope->GetC2SpotOffset(cIndex)) /
      mScope->GetC2IntensityFactor();
  }
  mScope->SetIntensity(cDelISY);
  cDelISY = mScope->FastIntensity();
  cDelISX = mScope->GetC2Percent(cIndex, cDelISY);
  mLogRpt.Format("Intensity set to %.3f%s  -  %.5f", cDelISX, mScope->GetC2Units(),
    cDelISY);
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
  cIndex = mScope->GetJeolGIF();
  mLogRpt.Format("JEOL GIF MODE return value %d", cIndex);
  SetReportedValues(&mStrItems[1], (double)cIndex);
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
  if (mItemInt[1] & 1)
    cIndex = mScope->NormalizeProjector();
  if (mItemInt[1] & 2)
    cIndex = mScope->NormalizeObjective();
  if (mItemInt[1] & 4)
    cIndex = mScope->NormalizeCondenser();
  if (!cIndex)
    AbortMacro();
  return 0;
}

// NormalizeAllLenses
int CMacCmd::NormalizeAllLenses(void)
{
  cIndex = 0;
  if (!mItemEmpty[1])
    cIndex = mItemInt[1];
  if (cIndex < 0 || cIndex > 3)
    ABORT_LINE("Lens group specifier must be between 0 and 3 in: \n\n");
  if (!mScope->NormalizeAll(cIndex))
    AbortMacro();
  return 0;
}

// ReportSlotStatus
int CMacCmd::ReportSlotStatus(void)
{
  if (!mScope->CassetteSlotStatus(mItemInt[1], cIndex)) {
    AbortMacro();
    return 1;
  }
  if (cIndex < -1)
    mLogRpt.Format("Requesting status of slot %d gives an error", mItemInt[1]);
  else
    mLogRpt.Format("Slot %d %s", mItemInt[1], cIndex < 0 ? "has unknown status" :
    (cIndex ? "is occupied" : "is empty"));
  SetReportedValues(&mStrItems[2], (double)cIndex);
  return 0;
}

// LoadCartridge, UnloadCartridge
int CMacCmd::LoadCartridge(void)
{
  if (CMD_IS(LOADCARTRIDGE))
    cIndex = mScope->LoadCartridge(mItemInt[1]);
  else
    cIndex = mScope->UnloadCartridge();
  if (cIndex)
    ABORT_LINE(cIndex == 1 ? "The thread is already busy for a long operation in:\n\n" :
    "There was an error trying to run a long operation with:\n\n");
  mStartedLongOp = true;
  return 0;
}

// RefrigerantLevel
int CMacCmd::RefrigerantLevel(void)
{
  if (mItemInt[1] < 1 || mItemInt[1] > 3)
    ABORT_LINE("Dewar number must be between 1 and 3 in: \n\n");
  if (!mScope->GetRefrigerantLevel(mItemInt[1], cDelX)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Refrigerant level in dewar %d is %.3f", mItemInt[1], cDelX);
  SetReportedValues(&mStrItems[2], cDelX);
  return 0;
}

// DewarsRemainingTime
int CMacCmd::DewarsRemainingTime(void)
{
  if (!mScope->GetDewarsRemainingTime(cIndex)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Remaining time to fill dewars is %d", cIndex);
  SetReportedValues(&mStrItems[1], (double)cIndex);
  return 0;
}

// AreDewarsFilling
int CMacCmd::AreDewarsFilling(void)
{
  if (!mScope->AreDewarsFilling(cIndex)) {
    AbortMacro();
    return 1;
  }
  if (FEIscope)
    mLogRpt.Format("Dewars %s busy filling", cIndex ? "ARE" : "are NOT");
  else {
    char *dewarTxt[4] = {"No tanks are", "Stage tank is", "Transfer tank is",
      "Stage and transfer tanks are"};
    B3DCLAMP(cIndex, 0, 3);
    mLogRpt.Format("%s being refilled", dewarTxt[cIndex]);
  }
  SetReportedValues(&mStrItems[1], cIndex);
  return 0;
}

// ReportVacuumGauge
int CMacCmd::ReportVacuumGauge(void)
{
  if (!mScope->GetGaugePressure((LPCTSTR)mStrItems[1], cIndex, cDelISX)) {
    if (JEOLscope && cIndex < -1 && cIndex > -4) {
      cReport.Format("Name must be \"Pir\" or \"Pen\" followed by number between 0 and "
        "9 in line:\n\n");
      ABORT_LINE(cReport);
    }
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Gauge %s status is %d, pressure is %f", (LPCTSTR)mStrItems[1], cIndex,
    cDelISX);
  SetReportedValues(&mStrItems[2], cIndex, cDelISX);
  return 0;
}

// ReportHighVoltage
int CMacCmd::ReportHighVoltage(void)
{
  cDelISX = mScope->GetHTValue();
  mLogRpt.Format("High voltage is %.1f kV", cDelISX);
  SetReportedValues(&mStrItems[1], cDelISX);
  return 0;
}

// SetSlitWidth
int CMacCmd::SetSlitWidth(void)
{
  if (mItemEmpty[1])
    ABORT_LINE("SetSlitWidth must be followed by a number in: \n\n");
  cDelISX = mItemDbl[1];
  if (cDelISX < mFiltParam->minWidth || cDelISX > mFiltParam->maxWidth)
    ABORT_LINE("This is not a legal slit width in: \n\n");
  mFiltParam->slitWidth = (float)cDelISX;
  mWinApp->mFilterControl.UpdateSettings();
  mCamera->SetupFilter();
  UpdateLDAreaIfSaved();
  return 0;
}

// SetEnergyLoss, ChangeEnergyLoss
int CMacCmd::SetEnergyLoss(void)
{
  if (mItemEmpty[1])
    ABORT_LINE(mStrItems[0] + " must be followed by a number in: \n\n");
  cDelISX = mItemDbl[1];
  if (CMD_IS(CHANGEENERGYLOSS))
    cDelISX += mFiltParam->energyLoss;
  if (mWinApp->mFilterControl.LossOutOfRange(cDelISX, cDelISY)) {
    cReport.Format("The energy loss requested in:\n\n%s\n\nrequires a net %s of %.1f"
      "with the current adjustments.\nThis net value is beyond the allowed range.",
      (LPCTSTR)mStrLine, mScope->GetHasOmegaFilter() ? "shift" : "offset", cDelISY);
    ABORT_LINE(cReport);
  }
  mFiltParam->energyLoss = (float)cDelISX;
  mFiltParam->zeroLoss = false;
  mWinApp->mFilterControl.UpdateSettings();
  mCamera->SetupFilter();
  UpdateLDAreaIfSaved();
  return 0;
}

// SetSlitIn
int CMacCmd::SetSlitIn(void)
{
  cIndex = 1;
  if (!mItemEmpty[1])
    cIndex = mItemInt[1];
  mFiltParam->slitIn = cIndex != 0;
  mWinApp->mFilterControl.UpdateSettings();
  mCamera->SetupFilter();
  UpdateLDAreaIfSaved();
  return 0;
}

// RefineZLP
int CMacCmd::RefineZLP(void)
{
  if (mItemEmpty[1] || SEMTickInterval(1000. * mFiltParam->alignZLPTimeStamp) >
      60000. *mItemDbl[1]) {
    CTime ctdt = CTime::GetCurrentTime();
    cReport.Format("%02d:%02d:%02d", ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond());
    mWinApp->AppendToLog(cReport, LOG_OPEN_IF_CLOSED);
    mWinApp->mFilterTasks->RefineZLP(false);
  }
  return 0;
}

// SelectCamera
int CMacCmd::SelectCamera(void)
{
  cIndex = mItemInt[1];
  if (cIndex < 1 || cIndex > mWinApp->GetNumActiveCameras())
    ABORT_LINE("Camera number out of range in: \n\n");
  RestoreCameraSet(-1, true);
  mWinApp->SetActiveCameraNumber(cIndex - 1);
  return 0;
}

// SetExposure
int CMacCmd::SetExposure(void)
{
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], cIndex, mStrCopy))
    ABORT_LINE(mStrCopy);
  cDelX = mItemDbl[2];
  cDelY = mItemEmpty[3] ? 0. : mItemDbl[3];
  if (mItemEmpty[2] || cDelX <= 0. || cDelY < 0.)
    ABORT_LINE("Incorrect entry for setting exposure: \n\n");
  SaveControlSet(cIndex);
  mConSets[cIndex].exposure = (float)cDelX;
  if (!mItemEmpty[3])
    mConSets[cIndex].drift = (float)cDelY;
  return 0;
}

// SetBinning
int CMacCmd::SetBinning(void)
{
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], cIndex, mStrCopy))
    ABORT_LINE(mStrCopy);
  if (CheckCameraBinning(mItemDbl[2], cIndex2, mStrCopy))
    ABORT_LINE(mStrCopy);
  SaveControlSet(cIndex);
  mConSets[cIndex].binning = cIndex2;
  return 0;
}

// SetCameraArea
int CMacCmd::SetCameraArea(void)
{
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], cIndex, mStrCopy))
    ABORT_LINE(mStrCopy);
  cReport = mStrItems[2];
  cReport.MakeUpper();
  if (cReport == "F" || cReport == "H" || cReport == "Q" ||
    cReport == "WH" || cReport == "WQ") {
    if (cReport == "F" || cReport == "WH") {
      cIx0 = 0;
      cIx1 = mCamParams->sizeX;
    } else if (cReport == "Q") {
      cIx0 = 3 * mCamParams->sizeX / 8;
      cIx1 = 5 * mCamParams->sizeX / 8;
    } else {
      cIx0 = mCamParams->sizeX / 4;
      cIx1 = 3 * mCamParams->sizeX / 4;
    }
    if (cReport == "F") {
      cIy0 = 0;
      cIy1 = mCamParams->sizeY;
    } else if (cReport == "H" || cReport == "WH") {
      cIy0 = mCamParams->sizeY / 4;
      cIy1 = 3 * mCamParams->sizeY / 4;
    } else {
      cIy0 = 3 * mCamParams->sizeY / 8;
      cIy1 = 5 * mCamParams->sizeY / 8;
    }
  } else {
    if (mItemEmpty[5])
      ABORT_LINE("Not enough coordinates for setting camera area: \n\n");
    cIndex2 = BinDivisorI(mCamParams);
    cIx0 = B3DMAX(0, B3DMIN(mCamParams->sizeX - 4, cIndex2 * mItemInt[2]));
    cIx1 = B3DMAX(cIx0 + 1, B3DMIN(mCamParams->sizeX, cIndex2 * mItemInt[3]));
    cIy0 = B3DMAX(0, B3DMIN(mCamParams->sizeY - 4, cIndex2 * mItemInt[4]));
    cIy1 = B3DMAX(cIy0 + 1, B3DMIN(mCamParams->sizeY, cIndex2 * mItemInt[5]));
  }
  cIndex2 = mConSets[cIndex].binning;
  cIy0 /= cIndex2;
  cIy1 /= cIndex2;
  cIx0 /= cIndex2;
  cIx1 /= cIndex2;
  cSizeX = cIx1 - cIx0;
  cSizeY = cIy1 - cIy0;
  mCamera->AdjustSizes(cSizeX, mCamParams->sizeX, mCamParams->moduloX, cIx0, cIx1, cSizeY,
    mCamParams->sizeY, mCamParams->moduloY, cIy0, cIy1, cIndex2);
  SaveControlSet(cIndex);
  mConSets[cIndex].left = cIx0 * cIndex2;
  mConSets[cIndex].right = cIx1 * cIndex2;
  mConSets[cIndex].top = cIy0 * cIndex2;
  mConSets[cIndex].bottom = cIy1 * cIndex2;
  return 0;
}

// SetCenteredSize
int CMacCmd::SetCenteredSize(void)
{
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], cIndex, mStrCopy))
    ABORT_LINE(mStrCopy);
  if (CheckCameraBinning(mItemDbl[2], cIndex2, mStrCopy))
    ABORT_LINE(mStrCopy);
  cSizeX = mItemInt[3];
  cSizeY = mItemInt[4];
  if (cSizeX < 4 || cSizeX * cIndex2 > mCamParams->sizeX || cSizeY < 4 ||
    cSizeY *cIndex2 > mCamParams->sizeY)
    ABORT_LINE("Image size is out of range for current camera at given binning in:"
    " \n\n");
  SaveControlSet(cIndex);
  mConSets[cIndex].binning = cIndex2;
  mCamera->CenteredSizes(cSizeX, mCamParams->sizeX, mCamParams->moduloX, cIx0, cIx1,
    cSizeY, mCamParams->sizeY, mCamParams->moduloY, cIy0, cIy1, cIndex2);
  mConSets[cIndex].left = cIx0 * cIndex2;
  mConSets[cIndex].right = cIx1 * cIndex2;
  mConSets[cIndex].top = cIy0 * cIndex2;
  mConSets[cIndex].bottom = cIy1 * cIndex2;
  return 0;
}

// SetExposureForMean
int CMacCmd::SetExposureForMean(void)
{
  cIndex = RECORD_CONSET;
  if (!mImBufs->mImage || mImBufs->IsProcessed() ||
    (mWinApp->LowDoseMode() && mCamera->ConSetToLDArea(mImBufs->mConSetUsed) != cIndex))
    ABORT_LINE("Script aborted setting exposure time because\n"
    " there is no image or a processed image in Buffer A\n"
    "(or, if in Low Dose mode, because the image in\n"
    " Buffer A is not from the Record area) for line:\n\n");
  if (mItemDbl[1] <= 0)
    ABORT_LINE("Exposure time must be positive for line:\n\n");
  cDelISX = mItemDbl[1] / mProcessImage->EquivalentRecordMean(0);
  cDelISY = cDelISX * B3DMAX(0.001, mConSets[cIndex].exposure - mCamParams->deadTime) +
    mCamParams->deadTime;
  if (mItemInt[2]) {

    // Adjusting frame time to keep constant number of frames
    if (!mCamParams->K2Type || !mConSets[cIndex].doseFrac)
      ABORT_LINE("Frame time can be adjusted only for K2/K3 camera"
      " with dose fractionation mode on in line:\n\n");
    cIndex2 = B3DNINT(mConSets[cIndex].exposure /
      B3DMAX(0.001, mConSets[cIndex].frameTime));
    cBmin = (float)(cDelISY / cIndex2);
    mCamera->ConstrainFrameTime(cBmin, mCamParams);
    if (fabs(cBmin - mConSets[cIndex].frameTime) < 0.0001) {
      PrintfToLog("In SetExposureForMean %s, change by a factor of %.4f would require "
        "too small a change in frame time", (LPCTSTR)mStrItems[1], cDelISX);
      cBmean = 0.;
    } else {
      SaveControlSet(cIndex);
      mConSets[cIndex].frameTime = cBmin;
      cBmean = cIndex2 * cBmin;
      PrintfToLog("In SetExposureForMean %s, frame time changed to %.4f, exposure time"
        " changed to %.4f", (LPCTSTR)mStrItems[1], mConSets[cIndex].frameTime,
        cBmean);
    }
  } else {

    // Just adjusting exposure time
    cBmean = (float)cDelISY;
    cIx1 = mCamera->DESumCountForConstraints(mCamParams, &mConSets[cIndex]);
    mCamera->CropTietzSubarea(mCamParams, mConSets[cIndex].right - mConSets[cIndex].left,
      mConSets[cIndex].bottom - mConSets[cIndex].top, mConSets[cIndex].processing,
      mConSets[cIndex].mode, cIy1);
    mCamera->ConstrainExposureTime(mCamParams, mConSets[cIndex].doseFrac,
      mConSets[cIndex].K2ReadMode, mConSets[cIndex].binning,
      mCamera->MakeAlignSaveFlags(&mConSets[cIndex]), cIx1, cBmean,
      mConSets[cIndex].frameTime, cIy1, mConSets[cIndex].mode);
    if (fabs(cBmean - mConSets[cIndex].exposure) < 0.00001) {
      PrintfToLog("In SetExposureForMean %s, change by a factor of %.4f would require "
        "too small a change in exposure time", (LPCTSTR)mStrItems[1], cDelISX);
      cBmean = 0.;
    } else {
      SaveControlSet(cIndex);
      cBmean = mWinApp->mFalconHelper->AdjustSumsForExposure(mCamParams, 
        &mConSets[cIndex], cBmean);
      /* This is good for seeing how the distribute frames function works
      report.Format("Summed frame list:");
      for (index2 = 0; index2 < mConSets[index].summedFrameList.size(); index2++) {
        strCopy.Format(" %d", mConSets[index].summedFrameList[index2]);
        report+= strCopy;
      }
      mWinApp->AppendToLog(report); */
      PrintfToLog("In SetExposureForMean %s, exposure time changed to %.4f",
        (LPCTSTR)mStrItems[1], cBmean);
    }
  }

  // Commit the new exposure and report if change is not good
  if (cBmean) {
    float diffThresh = 0.05f;
    mConSets[cIndex].exposure = cBmean;
    if (fabs(cBmean - cDelISY) / cDelISY > diffThresh)
      PrintfToLog("WARNING: Desired exposure time (%.3f) differs from actual one "
      "(%.3f) by more than %d%%", cDelISY, cBmean, B3DNINT(100. * diffThresh));
  }
  return 0;
}

// SetContinuous
int CMacCmd::SetContinuous(void)
{
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], cIndex, mStrCopy))
    ABORT_LINE(mStrCopy);
  SaveControlSet(cIndex);
  mConSets[cIndex].mode = mItemInt[2] ? CONTINUOUS : SINGLE_FRAME;
  return 0;
}

// SetProcessing
int CMacCmd::SetProcessing(void)
{
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], cIndex, mStrCopy))
    ABORT_LINE(mStrCopy);
  if (mItemInt[2] < 0 || mItemInt[2] > 2)
    ABORT_LINE("Processing must 0, 1, or 2 in:\n\n");
  SaveControlSet(cIndex);
  mConSets[cIndex].processing = mItemInt[2];
  return 0;
}

// SetFrameTime, ChangeFrameAndExposure
int CMacCmd::SetFrameTime(void)
{
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], cIndex, mStrCopy))
    ABORT_LINE(mStrCopy);
  if (!mCamParams->K2Type && !mCamParams->canTakeFrames)
    ABORT_NOLINE("Frame time cannot be set for the current camera type");
  SaveControlSet(cIndex);
  cTruth = CMD_IS(CHANGEFRAMEANDEXPOSURE);
  if (cTruth) {
    cIx0 = B3DNINT(mCamSet->exposure / mCamSet->frameTime);
    mCamSet->frameTime *= mItemFlt[2];
  } else {
    mCamSet->frameTime = mItemFlt[2];
  }
  mCamera->CropTietzSubarea(mCamParams, mCamSet->right - mCamSet->left,
    mCamSet->bottom - mCamSet->top, mCamSet->processing,
    mCamSet->mode, cIy1);
  mCamera->ConstrainFrameTime(mCamSet->frameTime, mCamParams,
    mCamSet->binning, mCamParams->OneViewType ? mCamSet->K2ReadMode:cIy1);
  if (cTruth) {
    mCamSet->exposure = mCamSet->frameTime * cIx0;
    SetReportedValues(&mStrItems[3], mCamSet->frameTime, mCamSet->exposure);
  } else {
    SetReportedValues(&mStrItems[3], mCamSet->frameTime);
  }
  return 0;
}

// SetK2ReadMode
int CMacCmd::SetK2ReadMode(void)
{
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], cIndex, mStrCopy))
    ABORT_LINE(mStrCopy);
  if (!mCamParams->K2Type && !mCamParams->OneViewType && !IS_FALCON3_OR_4(mCamParams) &&
    !mCamParams->DE_camType)
    ABORT_NOLINE("Read mode cannot be set for the current camera type");
  if (mItemInt[2] < 0 || mItemInt[2] > 2)
    ABORT_LINE("Read mode must 0, 1, or 2 in:\n\n");
  SaveControlSet(cIndex);
  mConSets[cIndex].K2ReadMode = mItemInt[2];
  return 0;
}

// SetDoseFracParams
int CMacCmd::SetDoseFracParams(void)
{
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], cIndex, mStrCopy))
    ABORT_LINE(mStrCopy);
  if (!mItemEmpty[5] && (mItemInt[5] < 0 || mItemInt[5] > 2))
    ABORT_LINE("Alignment method (the fourth number) must be 0, 1, or 2 in:\n\n");
  SaveControlSet(cIndex);
  mConSets[cIndex].doseFrac = mItemInt[2] ? 1 : 0;
  if (!mItemEmpty[3])
    mConSets[cIndex].saveFrames = mItemInt[3] ? 1 : 0;
  if (!mItemEmpty[4])
    mConSets[cIndex].alignFrames = mItemInt[4] ? 1 : 0;
  if (!mItemEmpty[5])
    mConSets[cIndex].useFrameAlign = mItemInt[5];
  if (!mItemEmpty[6])
    mConSets[cIndex].sumK2Frames = mItemInt[6] ? 1 : 0;
  return 0;
}

// SetDECamFrameRate
int CMacCmd::SetDECamFrameRate(void)
{
  if (!(mCamParams->DE_camType && mCamParams->DE_FramesPerSec > 0))
    ABORT_LINE("The current camera must be a DE camera with adjustable frame rate for"
      " line:\n\n");
  cDelISX = mItemDbl[1];
  if (cDelISX < mCamParams->DE_MaxFrameRate + 1.)
    cDelISX = B3DMIN(cDelISX, mCamParams->DE_MaxFrameRate);
  if (cDelISX <= 0. || cDelISX > mCamParams->DE_MaxFrameRate) {
    cReport.Format("The new frame rate must be greater than zero\n"
      "and less than %.2f FPS for line:\n\n", mCamParams->DE_MaxFrameRate);
    ABORT_LINE(cReport);
  }
  if (mDEframeRateToRestore < 0) {
    mNumStatesToRestore++;
    mDEframeRateToRestore = mCamParams->DE_FramesPerSec;
    mDEcamIndToRestore = mCurrentCam;
  }
  mCamParams->DE_FramesPerSec = (float)cDelISX;
  mWinApp->mDEToolDlg.UpdateSettings();
  SetReportedValues(mDEframeRateToRestore);
  mLogRpt.Format("Changed frame rate of DE camera from %.2f to %.2f",
    mDEframeRateToRestore, cDelISX);
  return 0;
}

// UseContinuousFrames
int CMacCmd::UseContinuousFrames(void)
{
  cTruth = mItemInt[1] != 0;
  if ((mUsingContinuous ? 1 : 0) != (cTruth ? 1 : 0))
    mCamera->ChangePreventToggle(cTruth ? 1 : -1);
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
  cIndex = mCamera->DoingContinuousAcquire();
  if (cIndex)
    mLogRpt.Format("Continuous acquire is running with set %d", cIndex - 1);
  else
    mLogRpt = "Continuous acquire is not running";
  SetReportedValues(&mStrItems[1], cIndex - 1.);
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
  if (!mCamParams->STEMcamera)
    ABORT_LINE("The current camera is not a STEM camera in: \n\n");
  if (CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], cIndex, mStrCopy))
    ABORT_LINE(mStrCopy);
  cSizeX = mCamera->GetMaxChannels(mCamParams);
  if (mItemEmpty[cSizeX + 1])
    ABORT_LINE("There must be a detector number for each channel in: \n\n");
  cIndex2 = 0;
  for (cIx0 = 2; cIx0 < cSizeX + 2; cIx0++) {
    if (mItemInt[cIx0] < -1 || mItemInt[cIx0] >= mCamParams->numChannels)
      ABORT_LINE("Detector number out of range in: \n\n");
    for (cIy0 = cIx0 + 1; cIy0 < cSizeX + 2; cIy0++) {
      if (mItemInt[cIx0] == mItemInt[cIy0] && mItemInt[cIx0] != -1)
        ABORT_LINE("Duplicate detector number in: \n\n");
    }
    if (mItemInt[cIx0] >= 0)
      cIndex2++;
  }
  if (!cIndex2)
    ABORT_LINE("There must be at least one detector listed in: \n\n");
  SaveControlSet(cIndex);
  for (cIx0 = 0; cIx0 < cSizeX; cIx0++)
    mConSets[cIndex].channelIndex[cIx0] = mItemInt[cIx0 + 2];
  return 0;
}

// RestoreCameraSetCmd
int CMacCmd::RestoreCameraSetCmd(void)
{
  cIndex = -1;
  if (!mItemEmpty[1] && CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], cIndex,
    mStrCopy))
      ABORT_LINE(mStrCopy);
  if (RestoreCameraSet(cIndex, true))
    ABORT_NOLINE("No camera parameters were changed; there is nothing to restore");
  if (cIndex == RECORD_CONSET && mAlignWholeTSOnly) {
    SaveControlSet(cIndex);
    mConSets[cIndex].alignFrames = 0;
  }
  return 0;
}

// KeepCameraSetChanges
int CMacCmd::KeepCameraSetChanges(void)
{
  ControlSet *masterSets = mWinApp->GetCamConSets() +
    MAX_CONSETS *mCurrentCam;
  cIndex = -1;
  if (!mItemEmpty[1] && CheckAndConvertCameraSet(mStrItems[1], mItemInt[1], cIndex,
    mStrCopy))
    ABORT_LINE(mStrCopy);
  for (cIx0 = (int)mConsetNums.size() - 1; cIx0 >= 0; cIx0--) {
    if (mConsetNums[cIx0] == cIndex || cIndex < 0) {
      masterSets[mConsetNums[cIx0]] = mConSets[mConsetNums[cIx0]];
      mConsetsSaved.erase(mConsetsSaved.begin() + cIx0);
      mConsetNums.erase(mConsetNums.begin() + cIx0);
    }
  }
  return 0;
}

// ReportK2FileParams
int CMacCmd::ReportK2FileParams(void)
{
  cIndex = mCamera->GetK2SaveAsTiff();
  cIndex2 = mCamera->GetSaveRawPacked();
  cIx0 = mCamera->GetUse4BitMrcMode();
  cIx1 = mCamera->GetSaveTimes100();
  cIy0 = mCamera->GetSkipK2FrameRotFlip();
  cIy1 = mCamera->GetOneK2FramePerFile();
  mLogRpt.Format("File type %s  raw packed %d  4-bit mode %d  x100 %d  Skip rot %d  "
    "file/frame %d", cIndex > 1 ? "TIFF ZIP" : (cIndex ? "MRC" : "TIFF LZW"), cIndex2,
    cIx0, cIx1, cIy0, cIy1);
  SetReportedValues(&mStrItems[1], cIndex, cIndex2, cIx0, cIx1, cIy0, cIy1);
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
  CArray < FrameAliParams, FrameAliParams > *faParamArr = mCamera->GetFrameAliParams();
  FrameAliParams *faParam;
  BOOL *useGPUArr = mCamera->GetUseGPUforK2Align();
  cIndex = mConSets[RECORD_CONSET].faParamSetInd;
  if (cIndex < 0 || cIndex >= (int)faParamArr->GetSize())
    ABORT_LINE("The frame alignment parameter set index for Record is out of range "
      "for:\n\n");
  faParam = faParamArr->GetData() + cIndex;
  cIx1 = mCamParams->useSocket ? 1 : 0;
  if (CMD_IS(REPORTFRAMEALIPARAMS)) {
    cIndex = (faParam->doRefine ? 1 : -1) * faParam->refineIter;
    cIndex2 = (faParam->useGroups ? 1 : -1) * faParam->groupSize;
    mLogRpt.Format("Frame alignment for Record has %s %d, keep precision %d"
      ", strategy %d, all-vs-all %d, refine %d, group %d",
      faParam->binToTarget ? "target" : "binning",
      faParam->binToTarget ? faParam->targetSize : faParam->aliBinning,
      faParam->keepPrecision, faParam->strategy,
      faParam->numAllVsAll, cIndex, cIndex2);
    SetReportedValues(&mStrItems[1], faParam->aliBinning, faParam->keepPrecision,
      faParam->strategy, faParam->numAllVsAll, cIndex, cIndex2);

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
    cDelX = (faParam->truncate ? 1 : -1) * faParam->truncLimit;
    mLogRpt.Format("Frame alignment for Record has GPU %d, truncation %.2f, hybrid %d,"
      " filters %.4f %.4f %.4f", useGPUArr[cIx1], cDelX, faParam->hybridShifts,
      faParam->rad2Filt1, faParam->rad2Filt2, faParam->rad2Filt3);
    SetReportedValues(&mStrItems[1], useGPUArr[0], cDelX, faParam->hybridShifts,
      faParam->rad2Filt1, faParam->rad2Filt2, faParam->rad2Filt3);

  } else {                                              // SetFrameAli2
    useGPUArr[cIx1] = mItemInt[1] > 0;
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
  if (!(mCamera->IsDirectDetector(mCamParams) ||
    (mCamParams->canTakeFrames & FRAMES_CAN_BE_SAVED)))
    ABORT_LINE("Cannot save frames from the current camera for line:\n\n");
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  if (mCamParams->DE_camType || (mCamParams->FEItype && FCAM_ADVANCED(mCamParams))) {
    if (mStrCopy.FindOneOf("/\\") >= 0)
      ABORT_LINE("Only a single folder can be entered for this camera type in:\n\n");
  } else {
    cIndex = mStrCopy.GetAt(0);
    if (!(((cIndex == ' / ' || cIndex == '\\') &&
      (mStrCopy.GetAt(1) == ' / ' || mStrCopy.GetAt(1) == '\\')) ||
      ((mStrCopy.GetAt(2) == ' / ' || mStrCopy.GetAt(2) == '\\') && 
        mStrCopy.GetAt(1) == ':'
        && ((cIndex >= 'A' && cIndex <= 'Z') || (cIndex >= 'a' && cIndex <= 'z')))))
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
  cIndex = mItemEmpty[1] ? 1 : mItemInt[1];
  mSkipFrameAliCheck = cIndex > 0;
  return 0;
}

// ReportK3CDSmode
int CMacCmd::ReportK3CDSmode(void)
{
  cIndex = mCamera->GetUseK3CorrDblSamp() ? 1 : 0;
  mLogRpt.Format("CDS mode is %s", cIndex ? "ON" : "OFF");
  SetReportedValues(&mStrItems[1], cIndex);
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
  cIndex = mCamera->GetDivideBy2();
  cDelX = mCamera->GetCountScaling(mCamParams);
  if (mCamParams->K2Type == K3_TYPE)
    cDelX = mCamParams->countsPerElectron;
  SetReportedValues(&mStrItems[1], cIndex, cDelX);
  mLogRpt.Format("Division by 2 is %s; count scaling is %.3f", cIndex ? "ON" : "OFF",
    cDelX);
  return 0;
}

// SetDivideBy2
int CMacCmd::SetDivideBy2(void)
{
  mCamera->SetDivideBy2(mItemInt[1] != 0 ? 1 : 0);
  return 0;
}

// ReportNumFramesSaved
int CMacCmd::ReportNumFramesSaved(void)
{
  cIndex = mCamera->GetNumFramesSaved();
  SetReportedValues(&mStrItems[1], cIndex);
  mLogRpt.Format("Number of frames saved was %d", cIndex);
  return 0;
}

// CameraProperties
int CMacCmd::CameraProperties(void)
{
  if (!mItemEmpty[1]) {
    if (mItemInt[1] < 1 || mItemInt[1] > mWinApp->GetActiveCamListSize())
      ABORT_LINE("Active camera number is out of range in:\n\n")
    mCamParams = mWinApp->GetCamParams() + mActiveList[mItemInt[1] - 1];
  }
  cIx1 = BinDivisorI(mCamParams);
  cIndex = mCamParams->sizeX / cIx1;
  cIndex2 = mCamParams->sizeY / cIx1;
  if (mItemEmpty[2])
    cIy1 = mScope->GetMagIndex();
  else
    cIy1 = mItemInt[2];
  cDelX = 1000. * cIx1 * mShiftManager->GetPixelSize(
    mItemEmpty[1] ? mCurrentCam : mActiveList[mItemInt[1] - 1], cIy1);
  mLogRpt.Format("%s: size %d x %d    rotation/flip %d   pixel on chip %.1f   "
    "unbinned pixel %.3f nm at %dx",
    (LPCSTR)mCamParams->name, cIndex, cIndex2, mCamParams->rotationFlip,
    mCamParams->pixelMicrons *cIx1, cDelX, MagForCamera(mCamParams, cIy1));
  SetReportedValues((double)cIndex, (double)cIndex2,
    (double)mCamParams->rotationFlip, mCamParams->pixelMicrons * cIx1, cDelX);
  return 0;
}

// ReportColumnOrGunValve
int CMacCmd::ReportColumnOrGunValve(void)
{
  cIndex = mScope->GetColumnValvesOpen();
  if (cIndex == -2)
    ABORT_NOLINE("An error occurred getting the state of the column/gun valve");
  SetReportedValues(&mStrItems[1], cIndex);
  mLogRpt.Format("Column/gun valve state is %d", cIndex);
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
  cDelISX = mScope->GetFilamentCurrent();
  if (cDelISX < 0)
    ABORT_NOLINE("An error occurred getting the filament current");
  SetReportedValues(&mStrItems[1], cDelISX);
  mLogRpt.Format("Filament current is %.5g", cDelISX);
  return 0;
}

// SetFilamentCurrent
int CMacCmd::SetFilamentCurrent(void)
{
  if (!mScope->SetFilamentCurrent(mItemDbl[1]))
    ABORT_NOLINE("An error occurred setting the filament current");
  return 0;
}

// IsPVPRunning
int CMacCmd::IsPVPRunning(void)
{
  if (!mScope->IsPVPRunning(cTruth))
    ABORT_NOLINE("An error occurred determining whether the PVP is running");
  mLogRpt.Format("The PVP %s running", cTruth ? "IS" : "is NOT");
  SetReportedValues(&mStrItems[1], cTruth ? 1. : 0.);
  return 0;
}

// SetBeamBlank
int CMacCmd::SetBeamBlank(void)
{
  if (mItemEmpty[1])
    ABORT_LINE("Entry requires a number for setting blank on or off: \n\n");
  cIndex = mItemInt[1];
  mScope->BlankBeam(cIndex != 0);
  return 0;
}

// MoveToNavItem
int CMacCmd::MoveToNavItem(void)
{
  ABORT_NONAV;
  cIndex = -1;
  if (!mItemEmpty[1]) {
    cIndex = mItemInt[1] - 1;
    if (cIndex < 0)
      ABORT_LINE("The Navigator item index must be positive: \n\n");
  }
  if (mNavigator->MoveToItem(cIndex))
    ABORT_NOLINE("Error moving to Navigator item");
  mMovedStage = true;
  return 0;
}

// RealignToNavItem, RealignToOtherItem
int CMacCmd::RealignToNavItem(void)
{
  ABORT_NONAV;
  cTruth = CMD_IS(REALIGNTOOTHERITEM);
  cIndex2 = cTruth ? 2 : 1;
  cIndex = mItemInt[1] - 1;
  cBmax = 0.;
  cIx1 = cIx0 = 0;
  if (!mItemEmpty[cIndex2 + 2]) {
    if (mItemEmpty[cIndex2 + 4])
      ABORT_LINE("Entry requires three values for controlling image shift reset in:"
      "\n\n");
    cBmax = mItemFlt[cIndex2 + 2];
    cIx0 = mItemInt[cIndex2 + 3];
    cIx1 = mItemInt[cIndex2 + 4];
  }
  if (!mItemEmpty[cIndex2 + 1])
    mNavHelper->SetContinuousRealign(mItemInt[cIndex2 + 1]);
  if (cTruth)
    cIy0 = mNavigator->RealignToOtherItem(cIndex, mItemInt[cIndex2] != 0, cBmax, cIx0, 
      cIx1);
  else
    cIy0 = mNavigator->RealignToCurrentItem(mItemInt[cIndex2] != 0, cBmax, cIx0, cIx1);
  if (cIy0) {
    cReport.Format("Script halted due to failure %d in Realign to Item routine", cIy0);
    ABORT_NOLINE(cReport);
    mNavHelper->SetContinuousRealign(0);
  }
  return 0;
}

// RealignToMapDrawnOn
int CMacCmd::RealignToMapDrawnOn(void)
{
  cNavItem = CurrentOrIndexedNavItem(mItemInt[1], mStrLine);
  if (!cNavItem)
    return 1;
  if (!cNavItem->mDrawnOnMapID)
    ABORT_LINE("The specified item has no ID for being drawn on a map in line:\n\n");
  cIx0 = mNavHelper->RealignToDrawnOnMap(cNavItem, mItemInt[2] != 0);
  if (cIx0) {
    cReport.Format("Script halted due to failure %d in Realign to Item for line:\n\n",
      cIx0);
    ABORT_LINE(cReport);
  }
  return 0;
}

// GetRealignToItemError
int CMacCmd::GetRealignToItemError(void)
{
  ABORT_NONAV;
  mNavHelper->GetLastStageError(cBacklashX, cBacklashY, cBmin, cBmax);
  SetReportedValues(&mStrItems[1], cBacklashX, cBacklashY, cBmin, cBmax);
  return 0;
}

// ReportNavItem, ReportOtherItem, ReportNextNavAcqItem, LoadNavMap, LoadOtherMap
int CMacCmd::ReportNavItem(void)
{
  ABORT_NONAV;
  cTruth = CMD_IS(REPORTNEXTNAVACQITEM);
  if (CMD_IS(REPORTNAVITEM) || CMD_IS(LOADNAVMAP)) {
    cIndex = mNavigator->GetCurrentOrAcquireItem(cNavItem);
    if (cIndex < 0)
      ABORT_LINE("There is no current Navigator item for line:\n\n.");
    cIndex2 = 1;
  } else if (cTruth) {
    if (!mNavigator->GetAcquiring())
      ABORT_LINE("The Navigator must be acquiring for line:\n\n");
    cNavItem = mNavigator->FindNextAcquireItem(cIndex);
    if (cIndex < 0) {
      mWinApp->AppendToLog("There is no next item to be acquired", mLogAction);
      SetVariable("NAVINDEX", "-1", VARTYPE_REGULAR, -1, false);
      SetReportedValues(-1);
    }
  } else {
    if (mItemInt[1] < 0) {
      cIndex = mNavigator->GetNumNavItems() + mItemInt[1];
    } else
      cIndex = mItemInt[1] - 1;
    cNavItem = mNavigator->GetOtherNavItem(cIndex);
    if (!cNavItem)
      ABORT_LINE("Index is out of range in statement:\n\n");
    cIndex2 = 2;
  }

  if (CMD_IS(REPORTNAVITEM) || CMD_IS(REPORTOTHERITEM) || (cTruth && cIndex >= 0)) {
    mLogRpt.Format("%stem %d:  Stage: %.2f %.2f %2.f  Label: %s",
      cTruth ? "Next i" : "I", cIndex + 1, cNavItem->mStageX, cNavItem->mStageY,
      cNavItem->mStageZ, (LPCTSTR)cNavItem->mLabel);
    if (!cNavItem->mNote.IsEmpty())
      mLogRpt += "\r\n    Note: " + cNavItem->mNote;
    SetReportedValues(cIndex + 1., cNavItem->mStageX, cNavItem->mStageY,
      cNavItem->mStageZ, (double)cNavItem->mType);
    cReport.Format("%d", cIndex + 1);
    SetVariable("NAVINDEX", cReport, VARTYPE_REGULAR, -1, false);
    SetVariable("NAVLABEL", cNavItem->mLabel, VARTYPE_REGULAR, -1, false);
    SetVariable("NAVNOTE", cNavItem->mNote, VARTYPE_REGULAR, -1, false);
    SetVariable("NAVCOLOR", cNavItem->mColor, VARTYPE_REGULAR, -1, false);
    SetVariable("NAVREGIS", cNavItem->mRegistration, VARTYPE_REGULAR, -1, false);
    cIndex = atoi(cNavItem->mLabel);
    cReport.Format("%d", cIndex);
    SetVariable("NAVINTLABEL", cReport, VARTYPE_REGULAR, -1, false);
    mNavHelper->GetNumHolesFromParam(cIx0, cIx1, cIndex2);
    SetVariable("NAVNUMHOLES", cNavItem->mAcquire ? 
      mNavHelper->GetNumHolesForItem(cNavItem, cIndex2) : 0, VARTYPE_REGULAR, -1, false);
    if (mNavigator->GetAcquiring()) {
      cReport.Format("%d", mNavigator->GetNumAcquired() + (cTruth ? 2 : 1));
      SetVariable("NAVACQINDEX", cReport, VARTYPE_REGULAR, -1, false);
    }
  } else if (!cTruth) {
    if (cNavItem->mType != ITEM_TYPE_MAP)
      ABORT_LINE("The Navigator item is not a map for line:\n\n");
    if (ConvertBufferLetter(mStrItems[cIndex2], mBufferManager->GetBufToReadInto(),
      false, cIx0, cReport))
      ABORT_LINE(cReport);
    mNavigator->DoLoadMap(false, cNavItem, cIx0);
    mLoadingMap = true;
  }
  return 0;
}

// ReportItemAcquire
int CMacCmd::ReportItemAcquire(void)
{
  cIndex = mItemEmpty[1] ? 0 : mItemInt[1];
  cNavItem = CurrentOrIndexedNavItem(cIndex, mStrLine);
  if (!cNavItem)
    return 1;
 mLogRpt.Format("Navigator item %d has Acquire %s", cIndex + 1,
    (cNavItem->mAcquire == 0) ? "disabled" : "enabled");
  SetReportedValues(cNavItem->mAcquire);
  return 0;
}

// SetItemAcquire
int CMacCmd::SetItemAcquire(void)
{
  cIndex = mItemEmpty[1] ? 0 : mItemInt[1];
  cNavItem = CurrentOrIndexedNavItem(cIndex, mStrLine);
  if (!cNavItem)
    return 1;
  if (mNavigator->GetAcquiring() && cIndex >= mNavigator->GetAcquireIndex() &&
    cIndex <= mNavigator->GetEndingAcquireIndex())
    ABORT_NOLINE("When the Navigator is acquiring, you cannot set an\n"
      "item to Acquire within the range still being acquired");
  cTruth = (mItemEmpty[2] || (mItemInt[2] != 0));
  if (cTruth && cNavItem->mTSparamIndex >= 0)
    ABORT_LINE("You cannot turn on Acquire for an item set for a tilt series for "
      "line:\n\n")
  cNavItem->mAcquire = cTruth;
  mNavigator->UpdateListString(cIndex);
  mNavigator->Redraw();
  mLogRpt.Format("Navigator item %d Acquire set to %s", cIndex + 1,
    cTruth ? "enabled" : "disabled");
  mNavigator->SetChanged(true);
  mLoopInOnIdle = !mNavigator->GetAcquiring();
  return 0;
}

// NavIndexWithLabel, NavIndexWithNote
int CMacCmd::NavIndexWithLabel(void)
{
  ABORT_NONAV;
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  cTruth = CMD_IS(NAVINDEXWITHNOTE);
  mNavigator->FindItemWithString(mStrCopy, cTruth);
  cIndex = mNavigator->GetFoundItem() + 1;
  if (cIndex > 0)
    mLogRpt.Format("Item with %s %s has index %d", cTruth ? "note" : "label",
      (LPCTSTR)mStrCopy, cIndex);
  else
    mLogRpt.Format("No item has %s %s", cTruth ? "note" : "label", (LPCTSTR)mStrCopy);
  SetReportedValues(cIndex);
  return 0;
}

// NavIndexItemDrawnOn
int CMacCmd::NavIndexItemDrawnOn(void)
{
  cIndex = mItemInt[1];
  cNavItem = CurrentOrIndexedNavItem(cIndex, mStrLine);
  if (!cNavItem)
    return 1;
  cIndex2 = 0;
  if (!cNavItem->mDrawnOnMapID) {
    mLogRpt.Format("Navigator item %d does not have an ID for being drawn on a map",
      cIndex + 1);
  } else {
    cNavItem = mNavigator->FindItemWithMapID(cNavItem->mDrawnOnMapID, true);
    if (!cNavItem) {
      mLogRpt.Format("The map that navigator item %d was drawn on is no longer in the "
        "table", cIndex + 1);
    } else {
      cIndex2 = mNavigator->GetFoundItem() + 1;
      mLogRpt.Format("Navigator item %d was drawn on map item %d", cIndex + 1, cIndex2);
    }
  }
  SetReportedValues(cIndex2);
  return 0;
}

// NavItemFileToOpen
int CMacCmd::NavItemFileToOpen(void)
{
  cIndex = mItemInt[1];
  cNavItem = CurrentOrIndexedNavItem(cIndex, mStrLine);
  if (!cNavItem)
    return 1;
  cReport = cNavItem->mFileToOpen;
  if (cReport.IsEmpty()) {
    mLogRpt.Format("No file is set to be opened for Navigator item %d", cIndex + 1);
    cReport = "0";
  } else {
    mLogRpt.Format("File to open at Navigator item %d is: %s", cIndex + 1,
      (LPCTSTR)cReport);
  }
  SetOneReportedValue(cReport, 1);
  return 0;
}

// SetNavItemUserValue, ReportItemUserValue
int CMacCmd::SetNavItemUserValue(void)
{
  cIndex = mItemInt[1];
  cNavItem = CurrentOrIndexedNavItem(cIndex, mStrLine);
  if (!cNavItem)
    return 1;
  cIndex2 = mItemInt[2];
  if (cIndex2 < 1 || cIndex2 > MAX_NAV_USER_VALUES) {
    cReport.Format("The user value number must be between 1 and %d in line:\n\n",
      MAX_NAV_USER_VALUES);
    ABORT_LINE(cReport);
  }
  if (CMD_IS(SETNAVITEMUSERVALUE)) {
    SubstituteLineStripItems(mStrLine, 3, mStrCopy);
    mNavHelper->SetUserValue(cNavItem, cIndex2, mStrCopy);
    mNavigator->SetChanged(true);
  } else {
    if (mNavHelper->GetUserValue(cNavItem, cIndex2, mStrCopy)) {
      mLogRpt.Format("Navigator item %d has no user value # %d", cIndex + 1, cIndex2);
      mStrCopy = "none";
    } else {
      mLogRpt.Format("User value # %d for Navigator item %d is %s", cIndex2, cIndex + 1,
        (LPCTSTR)mStrCopy);
    }
    SetOneReportedValue(mStrCopy, 1);
  }
  return 0;
}

// SetMapAcquireState
int CMacCmd::SetMapAcquireState(void)
{
  ABORT_NONAV;
  cNavItem = mNavigator->GetOtherNavItem(mItemInt[1] - 1);
  if (!cNavItem)
    ABORT_LINE("Index is out of range in statement:\n\n");
  if (cNavItem->mType != ITEM_TYPE_MAP) {
    cReport.Format("Navigator item %d is not a map for line:\n\n", mItemInt[1]);
    ABORT_LINE(cReport);
  }
  if (mNavHelper->SetToMapImagingState(cNavItem, true))
    ABORT_LINE("Failed to set map imaging state for line:\n\n");
  return 0;
}

// RestoreState
int CMacCmd::RestoreState(void)
{
  cIndex = mNavHelper->GetTypeOfSavedState();
  if (cIndex == STATE_NONE) {
    cReport.Format("Cannot Restore State: no state has been saved");
    if (mItemInt[1])
      ABORT_LINE(cReport);
    mWinApp->AppendToLog(cReport, mLogAction);
  } else {
    if (cIndex == STATE_MAP_ACQUIRE)
      mNavHelper->RestoreFromMapState();
    else {
      mNavHelper->RestoreSavedState();
      if (mNavHelper->mStateDlg)
        mNavHelper->mStateDlg->Update();
    }
    if (mNavHelper->mStateDlg)
      mNavHelper->mStateDlg->DisableUpdateButton();
  }
  return 0;
}

// ReportNumNavAcquire
int CMacCmd::ReportNumNavAcquire(void)
{
  mNavHelper->CountAcquireItems(0, -1, cIndex, cIndex2);
  if (cIndex < 0)
    mLogRpt = "The Navigator is not open; there are no acquire items";
  else
    mLogRpt.Format("Navigator has %d Acquire items, %d Tilt Series items", cIndex, 
      cIndex2);
  SetReportedValues(&mStrItems[1], (double)cIndex, (double)cIndex2);
  return 0;
}

// ReportNumHoleAcquire
int CMacCmd::ReportNumHoleAcquire(void)
{
  int minHoles = mItemEmpty[1] ? 1 : B3DMAX(1, mItemInt[1]);
  int startInd = mItemEmpty[2] ? 0 : (mItemInt[2] - 1);
  int endInd = mItemEmpty[3] ? -1 : (mItemInt[3] - 1);
  mNavHelper->CountHoleAcquires(startInd, endInd, minHoles, cIndex, cIndex2);
  if (cIndex < 0)
    mLogRpt = "The Navigator is not open; there are no acquire items";
  else
    mLogRpt.Format("With a minimum of %d holes, Navigator would acquire %d holes at %d "
      "items", minHoles, cIndex2, cIndex);
  SetReportedValues((double)cIndex2, (double)cIndex);
  return 0;
}

// ReportNumTableItems
int CMacCmd::ReportNumTableItems(void)
{
  ABORT_NONAV;
  cIndex = mNavigator->GetNumberOfItems();
  mLogRpt.Format("Navigator table has %d items", cIndex);
  SetReportedValues(&mStrItems[1], (double)cIndex);
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
  mNavigator->DoSave();
  return 0;
}

// ReportIfNavOpen
int CMacCmd::ReportIfNavOpen(void)
{
  cIndex = 0;
  mLogRpt = "Navigator is NOT open";
  if (mWinApp->mNavigator) {
    cIndex = 1;
    mLogRpt = "Navigator IS open";
    if (mWinApp->mNavigator->GetCurrentNavFile()) {
      mLogRpt += "; file is defined";
      cIndex = 2;
    }
  }
  SetReportedValues(&mStrItems[1], (double)cIndex);
  return 0;
}

// ReadNavFile, MergeNavFile
int CMacCmd::ReadNavFile(void)
{
  cTruth = CMD_IS(MERGENAVFILE);
  if (cTruth) {
    ABORT_NONAV;
  } else  {
    mWinApp->mMenuTargets.OpenNavigatorIfClosed();
  }
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  if (!CFile::GetStatus((LPCTSTR)mStrCopy, cStatus))
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

// ChangeItemRegistration, ChangeItemColor, ChangeItemLabel, ChangeItemNote
int CMacCmd::ChangeItemRegistration(void)
{
  ABORT_NONAV;
  cIndex = mItemInt[1];
  cIndex2 = mItemInt[2];
  cNavItem = mNavigator->GetOtherNavItem(cIndex - 1);
  cReport.Format("The Navigator item index, %d, is out of range in:\n\n", cIndex);
  if (!cNavItem)
    ABORT_LINE(cReport);
  if (CMD_IS(CHANGEITEMREGISTRATION)) {
    cReport.Format("The Navigator item with index %d is a registration point in:\n\n",
      cIndex);
    if (cNavItem->mRegPoint)
      ABORT_LINE(cReport);
    if (mNavigator->ChangeItemRegistration(cIndex - 1, cIndex2, cReport))
      ABORT_LINE(cReport + " in line:\n\n");
  } else {
    if (CMD_IS(CHANGEITEMCOLOR)) {
      cReport.Format("The Navigator item color must be between 0 and %d in:\n\n",
        NUM_ITEM_COLORS - 1);
      if (cIndex2 < 0 || cIndex2 >= NUM_ITEM_COLORS)
        ABORT_LINE(cReport);
      cNavItem->mColor = cIndex2;
    } else if (CMD_IS(CHANGEITEMNOTE)) {
      SubstituteVariables(&mStrLine, 1, mStrLine);
      if (mItemEmpty[2])
        mStrCopy = "";
      else
        mParamIO->StripItems(mStrLine, 2, mStrCopy);
      cNavItem->mNote = mStrCopy;
    } else {
      SubstituteLineStripItems(mStrLine, 2, mStrCopy);
      cReport.Format("The Navigator label size must be no more than %d characters "
        "in:\n\n", MAX_LABEL_SIZE);
      if (mStrCopy.GetLength() > MAX_LABEL_SIZE)
        ABORT_LINE(cReport);
      cNavItem->mLabel = mStrCopy;
    }
    mNavigator->SetChanged(true);
    mNavigator->UpdateListString(cIndex - 1);
    mNavigator->Redraw();
  }
  return 0;
}

// SetItemTargetDefocus
int CMacCmd::SetItemTargetDefocus(void)
{
  cIndex = mItemInt[1];
  cNavItem = CurrentOrIndexedNavItem(cIndex, mStrLine);
  if (!cNavItem)
    return 1;
  if (!cNavItem->mAcquire && cNavItem->mTSparamIndex < 0)
    ABORT_LINE("Specified Navigator item is not marked for acquisition in line:\n\n");
  if (mItemFlt[2] < -20 || mItemFlt[2] > 0)
    ABORT_LINE("Target defocus must be between 0 and -20 in line:\n\n:");
  cNavItem->mTargetDefocus = mItemFlt[2];
  mNavigator->SetChanged(true);
  mLogRpt.Format("For Navigator item %d, target defocus set to %.2f", cIndex + 1,
    mItemFlt[2]);
  return 0;
}

// SetItemSeriesAngles
int CMacCmd::SetItemSeriesAngles(void)
{
  cIndex = mItemInt[1];
  cNavItem = CurrentOrIndexedNavItem(cIndex, mStrLine);
  if (!cNavItem)
    return 1;
  if (cNavItem->mTSparamIndex < 0)
    ABORT_LINE("The specified Navigator item is not marked for a tilt series"
      " in line:\n\n");
  if (mNavHelper->CheckTiltSeriesAngles(cNavItem->mTSparamIndex, mItemFlt[2], mItemFlt[3],
    mItemEmpty[4] ? EXTRA_NO_VALUE : mItemFlt[4], cReport))
    ABORT_LINE(cReport + "in line:\n\n");
  cNavItem->mTSstartAngle = mItemFlt[2];
  cNavItem->mTSendAngle = mItemFlt[3];
  mLogRpt.Format("For Navigator item %d, tilt series range set to %.1f to %.1f",
    cIndex + 1, mItemFlt[2], mItemFlt[3]);
  if (!mItemEmpty[4]) {
    cNavItem->mTSbidirAngle = mItemFlt[4];
    cReport.Format(", start %.1f", mItemFlt[4]);
    mLogRpt += cReport;
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
  ABORT_NONAV;
  if (!mNavigator->GetAcquiring())
    ABORT_NOLINE("The navigator must be acquiring to set a group ID to skip");
  if (mItemEmpty[1]) {
    cIndex2 = mNavigator->GetCurrentOrAcquireItem(cNavItem);
    cIndex = cNavItem->mGroupID;
  } else {
    cIndex = mItemInt[1];
  }
  mNavigator->SetGroupIDtoSkip(cIndex);
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
  ABORT_NONAV;
  cIndex2 = CMD_IS(UPDATEGROUPZ) ? 1 : 0;
  cIndex = mNavigator->GetCurrentOrAcquireItem(cNavItem);
  if (cIndex < 0)
    ABORT_NOLINE("There is no current Navigator item.");
  cIndex = mNavigator->DoUpdateZ(cIndex, cIndex2);
  if (cIndex == 3)
    ABORT_LINE("Current Navigator item is not in a group for statement:\n\n");
  if (cIndex)
    ABORT_LINE("Error updating Z of Navigator item in statement:\n\n");
  mNavigator->SetChanged(true);
  return 0;
}

// ReportGroupStatus
int CMacCmd::ReportGroupStatus(void)
{
  ABORT_NONAV;
  cIndex = mNavigator->GetCurrentOrAcquireItem(cNavItem);
  if (cIndex < 0)
    ABORT_NOLINE("There is no current Navigator item.");
  cIndex2 = cNavItem->mGroupID;
  cIndex = -1;
  cIx0 = mNavigator->CountItemsInGroup(cIndex2, cReport, mStrCopy, cIx1);
  if (mNavigator->GetAcquiring()) {
    cIndex = 0;
    if (cIndex2)
      cIndex = mNavigator->GetFirstInGroup() ? 1 : 2;
  }
  mLogRpt.Format("Group acquire status %d, group ID %d, # in group %d, %d set to acquire"
    , cIndex, cIndex2, cIx0, cIx1);
  SetReportedValues(&mStrItems[1], (double)cIndex, (double)cIndex2, (double)cIx0,
    (double)cIx1);
  return 0;
}

// ReportItemImageCoords
int CMacCmd::ReportItemImageCoords(void)
{
  cIndex = mItemEmpty[1] ? 0 : mItemInt[1];
  cNavItem = CurrentOrIndexedNavItem(cIndex, mStrLine);
  if (!cNavItem)
    return 1;
  if (ConvertBufferLetter(mStrItems[2], 0, true, cIndex2, cReport))
    ABORT_LINE(cReport);
  if (mWinApp->mMainView->GetMapItemsForImageCoords(&mImBufs[cIndex2], true)) {
    mWinApp->mMainView->GetItemImageCoords(&mImBufs[cIndex2], cNavItem, cFloatX,
      cFloatY);
    mImBufs[cIndex2].mImage->getSize(cSizeX, cSizeY);
    if (cFloatX >= 0 && cFloatY >= 0 && cFloatX <= (cSizeX - 1) &&
      cFloatY <= (cSizeY - 1)) {
      SetReportedValues(cFloatX, cFloatY, 1);
      mLogRpt.Format("Navigator item %d has image coordinates %.2f, %.2f on %c",
        cIndex + 1, cFloatX, cFloatY, cIndex2 + 65);
    } else {
      SetReportedValues(cFloatX, cFloatY, 0);
      mLogRpt.Format("Navigator item %d is outside of %c", cIndex + 1, cIndex2 + 65);
    }
  } else {
    SetReportedValues(0.0, 0.0, -1);
    mLogRpt.Format("Navigator item %d could not be transformed to %c", cIndex + 1,
      cIndex2 + 65);
  }
  return 0;
}

// NewMap
int CMacCmd::NewMap(void)
{
  ABORT_NONAV;
  mNavigator->SetSkipBacklashType(1);
  cIndex = 0;
  if (!mItemEmpty[1]) {
    cIndex = mItemInt[1];
    if (mItemEmpty[2])
      ABORT_LINE("There must be text for the Navigator note after the number in:\n\n");
    SubstituteLineStripItems(mStrLine, 2, mStrCopy);
  }
  if (mNavigator->NewMap(false, cIndex, mItemEmpty[1] ? NULL : &mStrCopy)) {
    mCurrentIndex = mLastIndex;
    SuspendMacro();
    return 1;
  }
  SetReportedValues(&mStrItems[1], (double)mNavigator->GetNumberOfItems());
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
  ABORT_NONAV;
  cIndex = mNavigator->GetCurrentOrAcquireItem(cNavItem);
  if (cIndex < 0)
    ABORT_NOLINE("There is no current Navigator item.");
  mScope->GetStagePosition(cStageX, cStageY, cStageZ);
  cShiftX = (float)cStageX;
  cShiftY = (float)cStageY;
  mScope->GetImageShift(cDelISX, cDelISY);
  cIndex2 = mScope->GetLowDoseArea();
  if (mWinApp->LowDoseMode() && (cIndex2 == VIEW_CONSET || cIndex2 == SEARCH_AREA)) {
    mWinApp->mLowDoseDlg.GetNetViewShift(cStageX, cStageY, cIndex2);
    cDelISX -= cStageX;
    cDelISY -= cStageY;
  }
  mNavigator->ConvertIStoStageIncrement(mScope->FastMagIndex(),
      mCurrentCam, cDelISX, cDelISY, (float)mScope->FastTiltAngle(),
      cShiftX, cShiftY);
  cShiftX -= cNavItem->mStageX;
  cShiftY -= cNavItem->mStageY;
  cSpecDist = sqrt(cShiftX * cShiftX + cShiftY * cShiftY);
  if (cSpecDist <= mItemDbl[1]) {
    mNavigator->ShiftItemsAtRegistration(cShiftX, cShiftY,
      cNavItem->mRegistration);
    PrintfToLog("Items at registration %d shifted by %.2f, %.2f",
      cNavItem->mRegistration, cShiftX, cShiftY);
    mNavigator->SetChanged(true);
  } else {
    PrintfToLog("Current stage position is too far from item position (%.2f microns);"
      "nothing was shifted", cSpecDist);
  }
  return 0;
}

// ShiftItemsByMicrons
int CMacCmd::ShiftItemsByMicrons(void)
{
  ABORT_NONAV;
  if (fabs(mItemDbl[1]) > 100. || fabs(mItemDbl[2]) > 100.)
    ABORT_LINE("You cannot shift items by more than 100 microns in:\n\n");
  cIndex = mNavigator->GetCurrentRegistration();
  if (!mItemEmpty[3]) {
    cIndex = mItemInt[3];
    cReport.Format("Registration number %d is out of range in:\n\n", cIndex);
    if (cIndex <= 0 || cIndex > MAX_CURRENT_REG)
      ABORT_LINE(cReport);
  }
  cIndex2 = mNavigator->ShiftItemsAtRegistration(mItemFlt[1], mItemFlt[2],
    cIndex);
  mLogRpt.Format("%d items at registration %d were shifted by %.2f, %.2f", cIndex2, 
    cIndex, mItemDbl[1], mItemDbl[2]);
  mNavigator->SetChanged(true);
  return 0;
}

// AlignAndTransformItems
int CMacCmd::AlignAndTransformItems(void)
{
  cIndex2 = mNavHelper->BufferForRotAlign(cIndex);
  if (cIndex != mNavigator->GetCurrentRegistration()) {
    cReport.Format("Image in buffer %c is at registration %d, not the current Navigator"
      " registration, %d, for line:\n\n", 'A' + cIndex2, cIndex,
      mNavigator->GetCurrentRegistration());
    ABORT_LINE(cReport);
  }
  cIx0 = CNavRotAlignDlg::CheckAndSetupReference();
  if (cIx0 < 0)
    ABORT_LINE("Cannot find a Navigator item corresponding to the image in the Read"
      " buffer for line:\n\n");
  if (cIx0 == cIndex)
    ABORT_LINE("The image in the Read buffer is at the current Navigator"
      " registration for line:\n\n");
  if (fabs(mItemDbl[2]) < 0.2 || fabs(mItemDbl[2]) > 50.)
    ABORT_LINE("Angular range to search must be between 0.2 and 50 degrees for "
      "line:\n\n");
  CNavRotAlignDlg::PrepareToUseImage();
  if (CNavRotAlignDlg::AlignToMap(mItemFlt[1], mItemFlt[2], cBacklashX)) {
    AbortMacro();
    return 1;
  }
  if (!mItemEmpty[3] && fabs(cBacklashX - mItemDbl[1]) > mItemDbl[3]) {
    mLogRpt.Format("The rotation found by alignment, %.1f degrees, is farther from\r\n"
      " the center angle than the specified limit, nothing was transformed", cBacklashX);
  } else {
    cIndex = CNavRotAlignDlg::TransformItemsFromAlignment();
    mLogRpt.Format("%d items transformed after alignment with rotation of %.1f degrees",
      cIndex, cBacklashX);
    mNavigator->SetChanged(true);
  }
  SetReportedValues(cBacklashX);
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
  ABORT_NONAV;
  cImBuf = mWinApp->mMainView->GetActiveImBuf();
  if (!mItemEmpty[1]) {
    if (ConvertBufferLetter(mStrItems[1], -1, true, cIndex, cReport))
      ABORT_LINE(cReport);
    cImBuf = &mImBufs[cIndex];
  }
  if (mNavHelper->mHoleFinderDlg->DoFindHoles(cImBuf)) {
    AbortMacro();
    return 1;
  }
  return 0;
}

// MakeNavPointsAtHoles
int CMacCmd::MakeNavPointsAtHoles(void)
{
  ABORT_NONAV;
  cIndex = mItemEmpty[1] || mItemInt[1] < 0 ? - 1 : mItemInt[1];
  if (cIndex > 2)
    ABORT_LINE("The layout type must be less than 3 for line:\n\n");
  cIndex = mNavHelper->mHoleFinderDlg->DoMakeNavPoints(cIndex,
    (float)((mItemEmpty[2] || mItemDbl[2] < -900000) ? EXTRA_NO_VALUE : mItemDbl[2]),
    (float)((mItemEmpty[3] || mItemDbl[3] < -900000) ? EXTRA_NO_VALUE : mItemDbl[3]),
    (float)((mItemEmpty[4] || mItemDbl[4] < 0.) ? - 1. : mItemDbl[4]) / 100.f,
    (float)((mItemEmpty[5] || mItemDbl[5] < 0.) ? - 1. : mItemDbl[5]) / 100.f);
  if (cIndex < 0) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Hole finder made %d Navigator points", cIndex);
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
  ABORT_NONAV;
  B3DCLAMP(mItemInt[1], 0, 2);
  cIndex = mNavHelper->mCombineHoles->CombineItems(mItemInt[1]);
  if (cIndex)
    ABORT_NOLINE("Error trying to combine hole for multiple Records:\n" +
      CString(mNavHelper->mCombineHoles->GetErrorMessage(cIndex)));
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

// SetHelperParams
int CMacCmd::SetHelperParams(void)
{
  mNavHelper->SetTestParams(&mItemDbl[1]);
  return 0;
}

// SetMontageParams
int CMacCmd::SetMontageParams(void)
{
  if (!mWinApp->Montaging())
    ABORT_LINE("Montaging must be on already to use this command:\n\n");
  if (mWinApp->mStoreMRC && mWinApp->mStoreMRC->getDepth() > 0 &&
    ((mItemInt[2] > 0) || (mItemInt[3] > 0) ||
    (mItemInt[4] > 0) || (mItemInt[5] > 0)))
      ABORT_LINE("Atfer writing to the file, you cannot change frame size or overlaps "
        " in line:\n\n");
  if (mItemInt[1] >= 0)
    mMontP->moveStage = mItemInt[1] > 0;
  if (mItemInt[4] > 0) {
    if (mItemInt[4] < mMontP->xOverlap * 2)
      ABORT_LINE("The X frame size is less than twice the overlap in statement:\n\n");
    mMontP->xFrame = mItemInt[4];
  }
  if (mItemInt[5] > 0) {
    if (mItemInt[5] < mMontP->yOverlap * 2)
      ABORT_LINE("The Y frame size is less than twice the overlap in statement:\n\n");
    mMontP->yFrame = mItemInt[5];
  }
  if (mItemInt[2] > 0) {
    if (mItemInt[2] > mMontP->xFrame / 2)
      ABORT_LINE("X overlap is more than half the frame size in statement:\n\n");
    mMontP->xOverlap = mItemInt[2];
  }
  if (mItemInt[3] > 0) {
    if (mItemInt[3] > mMontP->yFrame / 2)
      ABORT_LINE("Y overlap is more than half the frame size in statement:\n\n");
    mMontP->yOverlap = mItemInt[3];
  }
  if (!mItemEmpty[6] && mItemInt[6] >= 0)
    mMontP->skipCorrelations = mItemInt[6] != 0;
  if (!mItemEmpty[7] && mItemDbl[7] >= 0.5) {
    if (CheckCameraBinning(mItemDbl[7], cIndex, cReport))
      ABORT_LINE(cReport);
    mMontP->binning = cIndex;
  }
  mWinApp->mMontageWindow.UpdateSettings();
  return 0;
}

// ManualFilmExposure
int CMacCmd::ManualFilmExposure(void)
{
  cDelX = mItemDbl[1];
  mScope->SetManualExposure(cDelX);
  return 0;
}

// ExposeFilm, SpecialExposeFilm
int CMacCmd::ExposeFilm(void)
{
  cDelX = 0.;
  cDelY = 0.;
  cIndex = 0;
  if (CMD_IS(SPECIALEXPOSEFILM)) {
    cDelX = mItemDbl[1];
    if (!mItemEmpty[2]) cDelY = mItemDbl[2];
    if (cDelX < 0. || cDelY < 0. || mItemEmpty[1])
      ABORT_LINE("There must be one or two non-negative values in statement:\n\n");
    if (!mItemEmpty[3])
      cIndex = mItemInt[3];
  }
  if (!mScope->TakeFilmExposure(CMD_IS(SPECIALEXPOSEFILM), cDelX, cDelY, cIndex != 0)) {
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
  if (CheckAndConvertLDAreaLetter(mStrItems[1], 1, cIndex, mStrLine))
    return 1;
  mScope->GotoLowDoseArea(cIndex);
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
  cIndex = mWinApp->LowDoseMode() ? 1 : 0;
  if (cIndex != (mItemInt[1] ? 1 : 0))
    mWinApp->mLowDoseDlg.SetLowDoseMode(mItemInt[1] != 0);
  SetReportedValues(&mStrItems[2], (double)cIndex);
  return 0;
}

// SetAxisPosition, ReportAxisPosition
int CMacCmd::SetAxisPosition(void)
{
  if (CheckAndConvertLDAreaLetter(mStrItems[1], 1, cIndex, mStrLine))
    return 1;
  if ((cIndex + 1) / 2 != 1)
    ABORT_LINE("This command must be followed by F or T:\n\n");
  if (CMD_IS(REPORTAXISPOSITION)) {
    cDelX = mWinApp->mLowDoseDlg.ConvertOneIStoAxis(mLdParam[cIndex].magIndex,
      mLdParam[cIndex].ISX, mLdParam[cIndex].ISY);
    cIx0 = ((mConSets[cIndex].right + mConSets[cIndex].left) / 2 - mCamParams->sizeX / 2) 
      / BinDivisorI(mCamParams);
    cIy0 = ((mConSets[cIndex].bottom + mConSets[cIndex].top) / 2 - mCamParams->sizeY / 2)
      / BinDivisorI(mCamParams);
    cIndex2 = mWinApp->mLowDoseDlg.m_bRotateAxis ? mWinApp->mLowDoseDlg.m_iAxisAngle : 0;
    mLogRpt.Format("%s axis position %.2f microns, %d degrees; camera offset %d, %d "
      "unbinned pixels", mModeNames[cIndex], cDelX, cIndex2, cIx0, cIy0);
    SetReportedValues(&mStrItems[2], cDelX, (double)cIndex2, (double)cIx0, (double)cIy0);

  } else {
    cIndex2 = 0;
    if (fabs(mItemDbl[2]) > 19.)
      ABORT_LINE("The axis distance is too large in:\n\n");
    if (!mItemEmpty[3])
      cIndex2 = B3DNINT(UtilGoodAngle(mItemDbl[3]));
    cIx0 = (mScope->GetLowDoseArea() + 1) / 2;
    if (cIx0 == 1)
      mScope->GetLDCenteredShift(cDelX, cDelY);
    mWinApp->mLowDoseDlg.NewAxisPosition(cIndex, mItemDbl[2], cIndex2, !mItemEmpty[3]);
    if (cIx0 == 1)
      mScope->SetLDCenteredShift(cDelX, cDelY);
  }
  return 0;
}

// ReportLowDose
int CMacCmd::ReportLowDose(void)
{
  char *modeLets = "VFTRS";
  cIndex = mWinApp->LowDoseMode() ? 1 : 0;
  cIndex2 = mScope->GetLowDoseArea();
  mLogRpt.Format("Low Dose is %s%s%c", cIndex ? "ON" : "OFF",
    cIndex && cIndex2 >= 0 ? " in " : "", 
    cIndex && cIndex2 >= 0 ? modeLets[cIndex2] : ' ');
  SetReportedValues(&mStrItems[1], (double)cIndex, (double)cIndex2);
  return 0;
}

// CurrentSettingsToLDArea
int CMacCmd::CurrentSettingsToLDArea(void)
{
  if (CheckAndConvertLDAreaLetter(mStrItems[1], -1, cIndex, mStrLine))
    return 1;
  mWinApp->InitializeOneLDParam(mLdParam[cIndex]);
  mWinApp->mLowDoseDlg.SetLowDoseMode(true);
  mScope->GotoLowDoseArea(cIndex);
  return 0;
}

// UpdateLowDoseParams
int CMacCmd::UpdateLowDoseParams(void)
{
  if (mWinApp->mTSController->DoingTiltSeries())
    ABORT_NOLINE("You cannot use ChangeLowDoseParams during a tilt series");
  if (CheckAndConvertLDAreaLetter(mStrItems[1], 0, cIndex, mStrLine))
    return 1;
  if (!mKeepOrRestoreArea[cIndex]) {
    mLDParamsSaved.push_back(mLdParam[cIndex]);
    mLDareasSaved.push_back(cIndex);
    mKeepOrRestoreArea[cIndex] = (mItemEmpty[2] || !mItemInt[2]) ? 1 : -1;
    UpdateLDAreaIfSaved();
  }
  return 0;
}

// RestoreLowDoseParamsCmd
int CMacCmd::RestoreLowDoseParamsCmd(void)
{
  if (CheckAndConvertLDAreaLetter(mStrItems[1], 0, cIndex, mStrLine))
    return 1;
  RestoreLowDoseParams(cIndex);
  return 0;
}

// SetLDAddedBeamButton
int CMacCmd::SetLDAddedBeamButton(void)
{
  cTruth = mItemEmpty[1] || mItemInt[1];
  if (!mWinApp->LowDoseMode() || mScope->GetLowDoseArea() < 0)
    ABORT_LINE("You must be in Low Dose mode and in a defined area for line:\n\n");
  if (mLDSetAddedBeamRestore < 0) {
    mLDSetAddedBeamRestore = mWinApp->mLowDoseDlg.m_bSetBeamShift ? 1 : 0;
    mNumStatesToRestore++;
  }
  mWinApp->mLowDoseDlg.SetBeamShiftButton(cTruth);
  if ((cTruth ? 1 : 0) == mLDSetAddedBeamRestore) {
    mLDSetAddedBeamRestore = -1;
    mNumStatesToRestore--;
  }
  return 0;
}

// ShowMessageOnScope
int CMacCmd::ShowMessageOnScope(void)
{
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  if (!FEIscope)
    ABORT_LINE("This command can be run only on an FEI scope");
  if (mScope->GetPluginVersion() < FEI_PLUGIN_MESSAGE_BOX) {
    cReport.Format("You need to upgrade the FEI plugin and/or server\n"
      "to at least version %d get a message box in:\n\n", FEI_PLUGIN_MESSAGE_BOX);
    ABORT_LINE(cReport);
  }
  mScope->SetMessageBoxArgs(mBoxOnScopeType, mStrCopy, mBoxOnScopeText);
  cIy1 = LONG_OP_MESSAGE_BOX;
  cIndex = mScope->StartLongOperation(&cIy1, &mBoxOnScopeInterval, 1);
  if (cIndex > 0)
    ABORT_LINE("The thread is already busy for a long operation so cannot run:\n\n");
  if (!cIndex) {
    mStartedLongOp = true;
    mShowedScopeBox = true;
  } else {
    SetReportedValues(0.);
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
  cIndex = mCamera->UpdateK2HWDarkRef(mItemFlt[1]);
  if (cIndex == 1)
    ABORT_LINE("The thread is already busy for this operation:\n\n")
  mStartedLongOp = !cIndex;
  return 0;
}

// LongOperation
int CMacCmd::LongOperation(void)
{
  cIx1 = 0;
  cIy1 = 1;
  int used[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int operations[MAX_LONG_OPERATIONS + 1];
  float intervals[MAX_LONG_OPERATIONS + 1];
  for (cIndex = 1; cIndex < MAX_MACRO_TOKENS && !mItemEmpty[cIndex]; cIndex++) {
    for (cIndex2 = 0; cIndex2 < MAX_LONG_OPERATIONS; cIndex2++) {
      if (!mStrItems[cIndex].Left(2).CompareNoCase(CString(mLongKeys[cIndex2]))) {
        if (mLongHasTime[cIndex2] && mItemDbl[cIndex + 1] < -1.5) {
          cBacklashX = mItemFlt[cIndex + 1];
          mScope->StartLongOperation(&cIndex2, &cBacklashX, 1);
          SetOneReportedValue(cBacklashX, cIy1++);
          cReport.Format("Time since last long operation %s is %.2f hours",
            (LPCTSTR)mStrItems[cIndex], cBacklashX);
          mWinApp->AppendToLog(cReport, mLogAction);
          cIndex++;
          break;
        }
        if (used[cIndex2])
          ABORT_LINE("The same operation is specified twice in:\n\n");
        used[cIndex2]++;
        operations[cIx1] = cIndex2;
        if (cIndex2 == LONG_OP_HW_DARK_REF &&
          !mCamera->CanDoK2HardwareDarkRef(mCamParams, cReport))
          ABORT_LINE(cReport + " in line:\n\n");
        if (mLongHasTime[cIndex2]) {
          if (cIndex == MAX_MACRO_TOKENS - 1 || mItemEmpty[cIndex + 1])
            ABORT_LINE("The last operation must be followed by an interval in hours "
              "in:\n\n");
          cReport = mStrItems[cIndex + 1];
          if (cReport.MakeLower() != mStrItems[cIndex + 1]) {
            cReport.Format("%s must be followed by an interval in hours in:\n\n",
              (LPCTSTR)mStrItems[cIndex]);
            ABORT_LINE(cReport);
          }
          cIndex++;
          intervals[cIx1++] = mItemFlt[cIndex];
        }
        else
          intervals[cIx1++] = 0.;
        break;
      }
    }
    if (cIndex2 == MAX_LONG_OPERATIONS) {
      cReport.Format("%s is not an allowed entry for a long operation in:\n\n",
        (LPCTSTR)mStrItems[cIndex]);
      ABORT_LINE(cReport);
    }
  }
  cIndex = mScope->StartLongOperation(operations, intervals, cIx1);
  if (cIndex > 0)
    ABORT_LINE(cIndex == 1 ? "The thread is already busy for a long operation in:\n\n" :
      "A long scope operation can be done only on an FEI scope for:\n\n");
  mStartedLongOp = !cIndex;
  return 0;
}

// NewDEserverDarkRef
int CMacCmd::NewDEserverDarkRef(void)
{
  if (mWinApp->mGainRefMaker->MakeDEdarkRefIfNeeded(mItemInt[1], mItemFlt[2],
    cReport))
    ABORT_NOLINE(CString("Cannot make a new dark reference in DE server with "
    "NewDEserverDarkRef:\n") + cReport);
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

// NextProcessArgs
int CMacCmd::NextProcessArgs(void)
{
  SubstituteLineStripItems(mStrLine, 1, mNextProcessArgs);
  return 0;
}

// CreateProcess
int CMacCmd::CreateProcess(void)
{
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  cIndex = mWinApp->mExternalTools->RunCreateProcess(mStrCopy, mNextProcessArgs);
  mNextProcessArgs = "";
  if (cIndex)
    ABORT_LINE("Script aborted due to failure to run process for line:\n\n");
  return 0;
}

// ExternalToolArgPlace
int CMacCmd::ExternalToolArgPlace(void)
{
  mRunToolArgPlacement = mItemInt[1];
  return 0;
}

// RunExternalTool
int CMacCmd::RunExternalTool(void)
{
  SubstituteLineStripItems(mStrLine, 1, mStrCopy);
  cIndex = mWinApp->mExternalTools->RunToolCommand(mStrCopy, mNextProcessArgs,
    mRunToolArgPlacement);
  mNextProcessArgs = "";
  if (cIndex)
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
  cIndex = mScope->GetProbeMode();
  if (mBeamTiltXtoRestore[cIndex] > EXTRA_VALUE_TEST) {
    cReport = "There is a second SaveBeamTilt without a RestoreBeamTilt";
    if (FEIscope)
      cReport += " for " + CString(cIndex ? "micro" : "nano") + "probe mode";
    ABORT_NOLINE(cReport);
  }
  mScope->GetBeamTilt(mBeamTiltXtoRestore[cIndex], mBeamTiltYtoRestore[cIndex]);
  mNumStatesToRestore++;
  return 0;
}

// RestoreBeamTilt
int CMacCmd::RestoreBeamTilt(void)
{
  cIndex = mScope->GetProbeMode();
  if (mBeamTiltXtoRestore[cIndex] < EXTRA_VALUE_TEST) {
    cReport = "There is a RestoreBeamTilt, but beam tilt was not saved or has been "
      "restored already";
    if (FEIscope)
      cReport += " for " + CString(cIndex ? "micro" : "nano") + "probe mode";
    ABORT_NOLINE(cReport);
  }
  mScope->SetBeamTilt(mBeamTiltXtoRestore[cIndex], mBeamTiltYtoRestore[cIndex]);
  mNumStatesToRestore--;
  mBeamTiltXtoRestore[cIndex] = mBeamTiltYtoRestore[cIndex] = EXTRA_NO_VALUE;
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
  if (mWinApp->mPiezoControl->GetNumPlugins()) {
    if (mWinApp->mPiezoControl->GetXYPosition(cDelISX, cDelISY)) {
      AbortMacro();
      return 1;
    }
  } else {
    if (!mScope->GetPiezoXYPosition(cDelISX, cDelISY)) {
      AbortMacro();
      return 1;
    }
  }
  mLogRpt.Format("Piezo X/Y position is %6g, %6g", cDelISX, cDelISY);
  SetReportedValues(&mStrItems[1], cDelISX, cDelISY);
  return 0;
}

// ReportPiezoZ
int CMacCmd::ReportPiezoZ(void)
{
  if (mWinApp->mPiezoControl->GetZPosition(cDelISX)) {
    AbortMacro();
    return 1;
  }
  mLogRpt.Format("Piezo Z or only position is %6g", cDelISX);
  SetReportedValues(&mStrItems[1], cDelISX);
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

