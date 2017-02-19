// PiezoAndPPControl.cpp:  Routines for controlling piezos in general and phase plates
//
// Copyright (C) 2014 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "PiezoAndPPControl.h"

CPiezoAndPPControl::CPiezoAndPPControl(void)
{
  mNumPlugins = 0;
  mSelectedPiezo = -1;
  mSelectedPlug = -1;
  mMoveThread = NULL;
}

CPiezoAndPPControl::~CPiezoAndPPControl(void)
{
}

// Initialize: get program pointers, get plugin functions and number of piezo's per plugin
// which is the initialization call for them
void CPiezoAndPPControl::Initialize(void)
{
  int plug;
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mScope = mWinApp->mScope;
  mNumPlugins = mWinApp->mPluginManager->GetPiezoPlugins(&mPlugFuncs[0], &mPlugNames[0]);
  for (plug = 0; plug < mNumPlugins; plug++) {
    mNumPiezos[plug] = mPlugFuncs[plug]->GetNumberOfPiezos();
    if (mNumPiezos[plug] <= 0)
      PrintfToLog("WARNING: The piezo plugin %s failed to initialize", 
        (LPCTSTR)(*mPlugNames[plug]));
  }

  // Allow the select to be skipped if there is just one piezo
  if (mNumPlugins == 1 && mNumPiezos[0] == 1) {
    mSelectedPiezo = 0;
    mSelectedPlug = 0;
  }
}

// Select a specific piezo in a specific plugin
bool CPiezoAndPPControl::SelectPiezo(int plugNum, int piezoNum)
{
  CString mess;
  int err;
  if (!mNumPlugins) {
    SEMMessageBox("There are no piezo plugins loaded, so no piezo can be selected");
    return true;
  }
  if (plugNum < 0 || plugNum >= mNumPlugins) {
    mess.Format("Plugin number %d is out of range in call to SelectPiezo.\n"
      "It must be between 0 and %d", plugNum, mNumPlugins - 1);
    SEMMessageBox(mess);
    return true;
  }
  if (piezoNum < 0 || piezoNum >= mNumPiezos[plugNum]) {
    mess.Format("Piezo number %d is out of range in call to SelectPiezo.\n"
      "It must be between 0 and %d", piezoNum, mNumPiezos[plugNum] - 1);
    SEMMessageBox(mess);
    return true;
  }
  err = WaitForPiezoReady();
  if (err) {
    mess.Format("Timeout waiting for piezo %d%s to be ready before trying to select a"
    " piezo", mSelectedPiezo, (LPCTSTR)DrivenByMess(mSelectedPlug));
  } else {
    err = mPlugFuncs[plugNum]->SelectPiezo(piezoNum);
    if (err)
      mess.Format("An error (%d) occurred trying to select piezo %d%s",
        err, piezoNum, (LPCTSTR)DrivenByMess(plugNum));
  }
  if (err) {
    SEMMessageBox(mess);
    return true;
  }
  mSelectedPiezo = piezoNum;
  mSelectedPlug = plugNum;
  return false;
}

// Get the X/Y position of the selected piezo.
bool CPiezoAndPPControl::GetXYPosition(double &xpos, double &ypos)
{
  int err;
  if (CheckSelection("get X/Y"))
    return true;
  if (!mPlugFuncs[mSelectedPlug]->GetXYPosition || 
    (err = mPlugFuncs[mSelectedPlug]->GetXYPosition(&xpos, &ypos)) < 0) {
      ReportNoAxis("get X/Y");
      return true;
  }
  if (err > 0) {
    ReportAxisError(err, "get X/Y");
    return true;
  }
  ScalePosition(false, true, xpos, ypos);
  return false;
}

// Get the Z position of the selected piezo
bool CPiezoAndPPControl::GetZPosition(double &zpos)
{
  int err;
  if (CheckSelection("get Z or only"))
    return true;
  if (!mPlugFuncs[mSelectedPlug]->GetZPosition || 
    (err = mPlugFuncs[mSelectedPlug]->GetZPosition(&zpos)) < 0) {
      ReportNoAxis("get Z");
      return true;
  }
  if (err > 0) {
    ReportAxisError(err, "get Z or only");
    return true;
  }
  ScalePosition(true, true, zpos, zpos);
  return false;
}

// Set the X/Y position to absolute or incremental value (incremental defaults to false)
bool CPiezoAndPPControl::SetXYPosition(double xpos, double ypos, bool incremental)
{
  CString mess;
  double curX, curY;
  if (CheckSelection("set X/Y"))
    return true;

  // If incremental, get the current position and compute the absolute target
  if (incremental) {
    if (GetXYPosition(curX, curY))
      return true;
    xpos += curX;
    ypos += curY;
  }
  ScalePosition(false, false, xpos, ypos);
  StartMovementThread(xpos, ypos, false);
  return false;
}

// Set the Z or only axis position to absolute or incremental value 
// (incremental defaults to false)
bool CPiezoAndPPControl::SetZPosition(double zpos, bool incremental)
{
  CString mess;
  double curZ;
  if (CheckSelection("set Z or only"))
    return true;
  if (incremental) {
    if (GetZPosition(curZ))
      return true;
    zpos += curZ;
  }
  ScalePosition(true, false, zpos, zpos);
  StartMovementThread(zpos, 0., true);
  return false;
}

// Check whether there is a valid selection and throw error message if not
bool CPiezoAndPPControl::CheckSelection(const char *action)
{
  CString mess;
  int err = WaitForPiezoReady();
  if (!err && mSelectedPlug >= 0 && mSelectedPiezo >= 0)
    return false;
  if (err)
    mess.Format("Timeout waiting for piezo %d%s to be ready before trying to %s position", 
    mSelectedPiezo, (LPCTSTR)DrivenByMess(mSelectedPlug), action);
  else
    mess.Format("A specific piezo must be selected before trying to %s position", action);
  SEMMessageBox(mess);
  return true;
}

// If the axis function does not exist or returns -2, call this to generate error message
void CPiezoAndPPControl::ReportNoAxis(const char *action)
{
  CString mess;
  mess.Format("It is not possible to %s position for piezo %d%s",
    action, mSelectedPiezo, (LPCTSTR)DrivenByMess(mSelectedPlug));
  SEMMessageBox(mess);
}

// If there is an error performing an action, call this to generate message
void CPiezoAndPPControl::ReportAxisError(int err, const char *action)
{
  CString mess;
  mess.Format("An error (%d) occurred trying to %s position for piezo %d%s",
    err, action, mSelectedPiezo, (LPCTSTR)DrivenByMess(mSelectedPlug));
  SEMMessageBox(mess);
}

// If there is more than one piezo plugin, make text about which plugin to add to message
CString CPiezoAndPPControl::DrivenByMess(int plugNum)
{
  CString mess = "";
  if (mNumPlugins > 1)
    mess.Format(" driven by plugin %d (%s)", plugNum, mPlugNames[plugNum]);
  return mess;
}

void CPiezoAndPPControl::StartMovementThread(double pos1, double pos2, bool setZ) 
{
  mPTD.pos1 = pos1;
  mPTD.pos2 = pos2;
  mPTD.setZ = setZ;
  mPTD.plugFuncs = mPlugFuncs[mSelectedPlug];
  mPTD.errCode = 0;
  mMoveThread = AfxBeginThread(PiezoMoveProc, &mPTD, THREAD_PRIORITY_NORMAL, 0,
    CREATE_SUSPENDED);
  mMoveThread->m_bAutoDelete = false;
  mMoveThread->ResumeThread();
}

UINT CPiezoAndPPControl::PiezoMoveProc(LPVOID pParam)
{
  PiezoThreadData *ptd = (PiezoThreadData *)pParam;

  if (ptd->setZ) {
    if (!ptd->plugFuncs->SetZPosition)
      ptd->errCode = -2;
    else
      ptd->errCode = ptd->plugFuncs->SetZPosition(ptd->pos1);

  } else {
    if (!ptd->plugFuncs->SetXYPosition)
      ptd->errCode = -2;
    else
      ptd->errCode = ptd->plugFuncs->SetXYPosition(ptd->pos1, ptd->pos2);
  }
  return ptd->errCode != 0 ? 1 : 0;
}

// Static for an OnIdle call
int CPiezoAndPPControl::TaskPiezoBusy(void)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  return winApp->mPiezoControl->PiezoBusy();
}

// Real function for testing if piezo is busy and doing the error reports
int CPiezoAndPPControl::PiezoBusy(void)
{
  CString mess;
  int err = UtilThreadBusy(&mMoveThread);
  if (err < 0) {
    if (mPTD.errCode < -1) {
      ReportNoAxis(mPTD.setZ ? "set Z or only" : "set X/Y");
    } else if (mPTD.errCode == -1) {
      if (mPTD.setZ)
        mess.Format("Trying to go to a Z or only position (%6g) out of range\n"
          "for piezo %d%s", mPTD.pos1, mSelectedPiezo, 
          (LPCTSTR)DrivenByMess(mSelectedPlug));
      else
        mess.Format("Trying to go to an X/Y position (%6g, %6g) out of range\n"
          "for piezo %d%s", mPTD.pos1, mPTD.pos2, mSelectedPiezo, 
          (LPCTSTR)DrivenByMess(mSelectedPlug));
      SEMMessageBox(mess);
    } else if (mPTD.errCode > 0) {
      ReportAxisError(mPTD.errCode, mPTD.setZ ? "set Z or only" : "set X/Y");
    } else {
      mess.Format("Thread returned non-zero without setting error code when trying to set"
        "%s position for piezo %d%s", mPTD.setZ ? "Z or only" : "X/Y", mSelectedPiezo, 
          (LPCTSTR)DrivenByMess(mSelectedPlug));
      SEMMessageBox(mess);
    }
  }
  return err;
}

// Not sure where this fits in
void CPiezoAndPPControl::PiezoCleanup(int error)
{
  UtilThreadCleanup(&mMoveThread);
}

// Wait until the busy function returns non-busy, up to the timeOut (default 5000)
int CPiezoAndPPControl::WaitForPiezoReady(int timeOut)
{
  if (!mMoveThread)
    return 0;
  double startTime = GetTickCount();
  while (SEMTickInterval(startTime) < timeOut) {
    if (PiezoBusy() <= 0)
      return 0;
    Sleep(20);
  }
  return 1;
}

// Look up a scaling for the current plugin/piezo and the given axis, apply scale factor
// to go from microns to units, or inverse if unitsToUm is true
void CPiezoAndPPControl::ScalePosition(bool zAxis, bool unitsToUm, double &xpos, 
                                       double &ypos)
{
  PiezoScaling scaling;
  float scaleFac;
  if (LookupScaling(mSelectedPlug, mSelectedPiezo, zAxis, scaling)) {
    scaleFac = scaling.unitsPerMicron;
    if (unitsToUm)
      scaleFac  = 1.f / scaleFac;
    xpos *= scaleFac;
    if (!zAxis)
      ypos *= scaleFac;
  }
}

// Looks up a scaling for the given piezo in the given plugin, for Z axis or not.
// Returns true if a scaling is found
bool CPiezoAndPPControl::LookupScaling(int plugNum, int piezoNum, bool zAxis, 
                                       PiezoScaling &scaling)
{
  for (int ind = 0; ind < mScalings.GetSize(); ind++) {
    scaling = mScalings.GetAt(ind);
    if (plugNum == scaling.plugNum && piezoNum == scaling.piezoNum &&
      (zAxis ? 1 : 0) == (scaling.zAxis ? 1 : 0))
      return true;
  }
  return false;
}
