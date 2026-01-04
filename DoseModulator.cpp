// DoseModulator.cpp:    Interface to IDES dose modulator
//
// Copyright (C) 2003-2026 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "DoseModulator.h"
#include "CameraController.h"
#include "ExternalTools.h"
#include "MacroProcessor.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

#define PCT_CHECK_SECONDS 5.

CDoseModulator::CDoseModulator()
{
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mLastFrequencySet = 0;
  mLastDutyPctSet = 0.;
  mIsInitialized = false;
  mDutyPercentTime = -1.;
}


CDoseModulator::~CDoseModulator()
{
}

// Initialize means send an initial command to enable the modulation and set duty percent
// to 100, setting frequency to property value also
int CDoseModulator::Initialize(CString &errStr)
{
  int err;
  CString data, outStr;

  // Processes are being run through a shell interpreter that needs to have quotes to be
  // passed escaped
  data = "{\\\"EnableModulation\\\": true}";
  mIsInitialized = true;
  err = RunCommand("ModulationSettings/EnableModulation", (LPCSTR)data, errStr, outStr);
  if (err <= 0)
    err = SetDutyPercent(100., errStr);
  if (err > 0) {
    mIsInitialized = false;
    return 1;
  } else {
    mLastDutyPctSet = 100.;
    mLastFrequencySet = mWinApp->mCamera->GetEDMfrequency();
    return 0;
  }
}

// Set Duty percent, setting the frequency to the current property value if it doesn't
// match what was last set
int CDoseModulator::SetDutyPercent(float pct, CString &errStr)
{
  int err;
  CString data, outStr;
  int freq = mWinApp->mCamera->GetEDMfrequency();
  if (freq != mLastFrequencySet) {
    data.Format("{ \\\"FrequencyHz\\\" : %d}", freq);
    if (RunCommand("ModulationSettings/FrequencyHz", (LPCTSTR)data, errStr, outStr) > 0)
      return 1;
    mLastFrequencySet = freq;
  }
  data.Format("{  \\\"DutyPercent\\\" : %.1f}", pct);
  err = RunCommand("ModulationSettings/DutyPercent", (LPCTSTR)data, errStr, outStr);
  if (err <= 0)
    mLastDutyPctSet = pct;
    mDutyPercentTime = GetTickCount();
  return err;
}

// Get the duty percent
int CDoseModulator::GetDutyPercent(float & pct, CString & errStr)
{
  int err, ind;
  CString outStr, valStr;
  err = RunCommand("ModulationSettings/DutyPercent", NULL, errStr, outStr);
  if (err > 0)
    return err;

  // Analyze string for the value after :
  ind = outStr.Find("{");
  ind = outStr.Find(":", ind);
  if (ind < 0) {
    errStr = "Could not find : in output string from getting duty %: " + outStr;
    return 1;
  }
  valStr = outStr.Mid(ind + 1);
  ind = valStr.Find("}");
  if (ind < 0) {
    errStr = "Could not find } after : in output string from getting duty %: " + outStr;
    return 1;
  }

  // Strip } and get value
  valStr = valStr.Left(ind);
  pct = (float)atof((LPCTSTR)valStr);
  mLastDutyPctSet = pct;
  mDutyPercentTime = GetTickCount();
  return err;
}

// Runs a command with the given option to be appended to the url, and optional data
// returns err > 0 for true error, error < 0 (minus the exit code of curl) if there is
// valid output but curl exits with error
int CDoseModulator::RunCommand(const char *urlOpt, const char *data, CString &errStr,
  CString &output)
{
  CString url, argString, cmdString = "curl";
  int err;
  double ticks = GetTickCount();

  // The lack of quoting for the http string and the use of " quotes for the data string
  // were determined from DOS command prompt
  url.Format("http://%s:%d/EDM/%s", (LPCTSTR)mWinApp->mCamera->GetEDMserverIP(),
    mWinApp->mCamera->GetEDMserverPort(), urlOpt);
  if (data) {
    argString.Format("--location --request PUT %s --data \"%s\"", (LPCTSTR)url,
      data);

  } else {
    argString = "--location " + url;
  }

  // Run a process with piped output
  err = mWinApp->mExternalTools->RunCreateProcess(cmdString, argString, true, "", true,
    &errStr);
  if (err)
    return 1;

  // Get the output frmom the pipe and clean up process
  err = mWinApp->mExternalTools->WaitForDoneGetPipeOutput(output, 5000);
  mWinApp->mMacroProcessor->CleanupExternalProcess();
  if (err < 0) {
    errStr = output;
    return 1;
  }

  // Error output from curl, no { }
  if (output.GetLength() > 0 && (output.Find("{") < 0 || output.Find("}") < 0)) {
    errStr = "curl returned error: " + output;
    return 1;
  }

  // error exit and no output
  if (err && !output.GetLength()) {
    errStr.Format("Sending command to dose modulator, curl returned no output and "
      "exited with code %d", -err);
    return 1;
  }

  // return possible error code even if good output
  return -err;
}

float CDoseModulator::GetRecentDutyPercent()
{
  float pct;
  CString str;

  if (mDutyPercentTime < 0 ||
    SEMTickInterval(mDutyPercentTime) > 1000 * PCT_CHECK_SECONDS) {
    if (!GetDutyPercent(pct, str)) {
      pct = mLastDutyPctSet;
    }
  } else {
    pct = mLastDutyPctSet;
  }
  return pct;
}


