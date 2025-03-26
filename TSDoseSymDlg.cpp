// TSDoseSymDlg.cpp : Dialog to set parameters for dose-symmetric tilting
//
// Copyright (C) 2003-2019 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "TSDoseSymDlg.h"
#include "TSController.h"
#include "EMbufferManager.h"
#include "CameraController.h"

FloatVec CTSDoseSymDlg::mIdealAnglesToUse;
bool CTSDoseSymDlg::mIgnoreAnglesToUse = false;

// CTSDoseSymDlg dialog

CTSDoseSymDlg::CTSDoseSymDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_TSDOSESYM, pParent)
  , m_strGroupSize(_T(""))
  , m_bIncGroup(FALSE)
  , m_fIncGroupAngle(0)
  , m_strIncAmount(_T(""))
  , m_strMinForAnchor(_T(""))
  , m_bRunToEnd(FALSE)
  , m_fRunToEnd(0)
  , m_strSummary(_T(""))
  , m_bUseAnchor(FALSE)
  , m_iIncInterval(0)
  , m_bReorderFile(FALSE)
  , m_strAngleSummary(_T(""))
  , m_bSkipBacklash(FALSE)
  , m_bTrackOnBigTilt(FALSE)
  , m_iBigTiltToTrack(0)
{

}

CTSDoseSymDlg::~CTSDoseSymDlg()
{
}

void CTSDoseSymDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_STAT_GROUP_SIZE, m_strGroupSize);
  DDX_Control(pDX, IDC_SPIN_GROUP_SIZE, m_sbcGroupSize);
  DDX_Check(pDX, IDC_CHECK_INC_GROUP, m_bIncGroup);
  DDX_Control(pDX, IDC_EDIT_INC_GROUP_ANGLE, m_editIncGroupAngle);
  DDX_Text(pDX, IDC_EDIT_INC_GROUP_ANGLE, m_fIncGroupAngle);
  DDV_MinMaxFloat(pDX, m_fIncGroupAngle, 0, 90);
  DDX_Control(pDX, IDC_STAT_INC_ANGLE, m_statIncAngle);
  DDX_Control(pDX, IDC_STAT_INC_LABEL1, m_statIncLabel1);
  DDX_Control(pDX, IDC_STAT_INC_LABEL2, m_statIncLabel2);
  DDX_Control(pDX, IDC_STAT_INC_AMOUNT, m_statIncAmount);
  DDX_Text(pDX, IDC_STAT_INC_AMOUNT, m_strIncAmount);
  DDX_Control(pDX, IDC_SPIN_INC_AMOUNT, m_sbcIncAmount);
  DDX_Control(pDX, IDC_STAT_MIN_FOR_ANCHOR, m_statMinForAnchor);
  DDX_Text(pDX, IDC_STAT_MIN_FOR_ANCHOR, m_strMinForAnchor);
  DDX_Control(pDX, IDC_SPIN_MIN_FOR_ANCHOR, m_sbcMinForAnchor);
  DDX_Check(pDX, IDC_CHECK_RUN_TO_END, m_bRunToEnd);
  DDX_Control(pDX, IDC_EDIT_RUN_TO_END, m_editRunToEnd);
  DDX_Text(pDX, IDC_EDIT_RUN_TO_END, m_fRunToEnd);
  DDV_MinMaxFloat(pDX, m_fRunToEnd, 0, 90);
  DDX_Text(pDX, IDC_STAT_SUMMARY, m_strSummary);
  DDX_Control(pDX, IDC_CHECK_USE_ANCHOR, m_butUseAnchor);
  DDX_Check(pDX, IDC_CHECK_USE_ANCHOR, m_bUseAnchor);
  DDX_Control(pDX, IDC_EDIT_INC_INTERVAL, m_editIncInterval);
  DDX_Text(pDX, IDC_EDIT_INC_INTERVAL, m_iIncInterval);
  DDV_MinMaxInt(pDX, m_iIncInterval, 0, 80);
  DDX_Control(pDX, IDC_STAT_ANCHOR_TILTS, m_statAnchorTilts);
  DDX_Control(pDX, IDC_CHECK_REORDER_FILE, m_butReorderFile);
  DDX_Check(pDX, IDC_CHECK_REORDER_FILE, m_bReorderFile);
  DDX_Text(pDX, IDC_STAT_ANGLE_SUMMARY, m_strAngleSummary);
  DDX_Control(pDX, IDC_STATEVERY, m_statEvery);
  DDX_Check(pDX, IDC_CHECK_SKIP_BACKLASH, m_bSkipBacklash);
  DDX_Check(pDX, IDC_CHECK_TRACK_BIG_TILT, m_bTrackOnBigTilt);
  DDX_Text(pDX, IDC_EDIT_BIG_TILT, m_iBigTiltToTrack);
}


BEGIN_MESSAGE_MAP(CTSDoseSymDlg, CBaseDlg)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_GROUP_SIZE, OnDeltaposSpinGroupSize)
  ON_BN_CLICKED(IDC_CHECK_INC_GROUP, OnCheckIncGroup)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_INC_AMOUNT, OnDeltaposSpinIncAmount)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_MIN_FOR_ANCHOR, OnDeltaposSpinMinForAnchor)
  ON_BN_CLICKED(IDC_CHECK_RUN_TO_END, OnCheckRunToEnd)
  ON_EN_KILLFOCUS(IDC_EDIT_INC_GROUP_ANGLE, OnEnKillfocusEditIncGroupAngle)
  ON_EN_KILLFOCUS(IDC_EDIT_RUN_TO_END, OnEnKillfocusEditRunToEnd)
  ON_BN_CLICKED(IDC_CHECK_USE_ANCHOR, OnCheckUseAnchor)
  ON_EN_KILLFOCUS(IDC_EDIT_INC_INTERVAL, OnEnKillfocusEditIncInterval)
  ON_EN_KILLFOCUS(IDC_EDIT_BIG_TILT, OnEnKillfocusEditBigTilt)
  ON_BN_CLICKED(IDC_CHECK_TRACK_BIG_TILT, OnCheckTrackBigTilt)
END_MESSAGE_MAP()


// CTSDoseSymDlg message handlers
BOOL CTSDoseSymDlg::OnInitDialog()
{
  ControlSet *cset = mWinApp->GetConSets() + RECORD_CONSET;
  BOOL async = mWinApp->mBufferManager->GetSaveAsynchronously();
  CBaseDlg::OnInitDialog();
  CLEAR_RESIZE(mIdealAnglesToUse, float, 0);
  m_bIncGroup = mTSparam.dosymIncreaseGroups;
  m_bRunToEnd = mTSparam.dosymDoRunToEnd;
  m_fRunToEnd = mTSparam.dosymRunToEndAngle;
  m_fIncGroupAngle = mTSparam.dosymIncStartAngle;
  m_bUseAnchor = mTSparam.dosymAnchorIfRunToEnd;
  m_iIncInterval = mTSparam.dosymGroupIncInterval;
  m_strGroupSize.Format("%d", mTSparam.dosymBaseGroupSize);
  m_strIncAmount.Format("%d", mTSparam.dosymGroupIncAmount);
  m_strMinForAnchor.Format("%d", mTSparam.dosymMinUniForAnchor);
  m_bSkipBacklash = mTSparam.dosymSkipBacklash;
  m_bReorderFile = mWinApp->mTSController->GetReorderDoseSymFile();
  m_bTrackOnBigTilt = mTSparam.dosymTrackBigReversal;
  m_iBigTiltToTrack = mTSparam.dosymBigTiltToTrack;
  m_sbcGroupSize.SetRange(0, 1000);
  m_sbcGroupSize.SetPos(500);
  m_sbcIncAmount.SetRange(0, 1000);
  m_sbcIncAmount.SetPos(500);
  m_sbcMinForAnchor.SetRange(0, 1000);
  m_sbcMinForAnchor.SetPos(500);
  if (async) {
    if (mWinApp->StartedTiltSeries()) {
      async = !mWinApp->mTSController->GetFrameAlignInIMOD();
    } else {
      async = !(mWinApp->mCamera->IsConSetSaving(cset, RECORD_CONSET,
        mWinApp->GetActiveCamParam(), false) && cset->alignFrames &&
        cset->useFrameAlign > 1 && mWinApp->mCamera->GetAlignWholeSeriesInIMOD());
    }
  }
  if (async) {
    ReplaceWindowText(&m_butReorderFile, "synchronously", "in background");
    ReplaceWindowText(&m_butReorderFile, "block", "allow");
  }
  ManageEnables();
  ManageSummary();
  return TRUE;
}

void CTSDoseSymDlg::OnOK()
{
  SetFocus();
  UpdateData(true);
  mTSparam.dosymSkipBacklash = m_bSkipBacklash;
  mWinApp->mTSController->SetReorderDoseSymFile(m_bReorderFile);
  CBaseDlg::OnOK();
}


void CTSDoseSymDlg::OnDeltaposSpinGroupSize(NMHDR *pNMHDR, LRESULT *pResult)
{
  FormattedSpinnerValue(pNMHDR, pResult, 1, 20, mTSparam.dosymBaseGroupSize,
    m_strGroupSize, "%d");
  *pResult = 0;
  ManageSummary();
}

void CTSDoseSymDlg::OnCheckIncGroup()
{
  UpdateData(true);
  mTSparam.dosymIncreaseGroups = m_bIncGroup;
  ManageEnables();
  ManageSummary();
}

void CTSDoseSymDlg::OnDeltaposSpinIncAmount(NMHDR *pNMHDR, LRESULT *pResult)
{
  FormattedSpinnerValue(pNMHDR, pResult, 1, 10, mTSparam.dosymGroupIncAmount,
    m_strIncAmount, "%d");
  *pResult = 0;
  ManageSummary();
}

void CTSDoseSymDlg::OnDeltaposSpinMinForAnchor(NMHDR *pNMHDR, LRESULT *pResult)
{
  FormattedSpinnerValue(pNMHDR, pResult, 1, 50, mTSparam.dosymMinUniForAnchor,
    m_strMinForAnchor, "%d");
  *pResult = 0;
}

void CTSDoseSymDlg::OnCheckRunToEnd()
{
  UpdateData(true);
  mTSparam.dosymDoRunToEnd = m_bRunToEnd;
  ManageEnables();
  ManageSummary();
}

void CTSDoseSymDlg::OnEnKillfocusEditIncGroupAngle()
{
  UpdateData(true);
  mTSparam.dosymIncStartAngle = m_fIncGroupAngle;
  ManageSummary();
}


void CTSDoseSymDlg::OnEnKillfocusEditRunToEnd()
{
  UpdateData(true);
  mTSparam.dosymRunToEndAngle = m_fRunToEnd;
  ManageSummary();
}

void CTSDoseSymDlg::OnCheckUseAnchor()
{
  UpdateData(true);
  mTSparam.dosymAnchorIfRunToEnd = m_bUseAnchor;
  ManageEnables();
}

void CTSDoseSymDlg::OnEnKillfocusEditIncInterval()
{
  UpdateData(true);
  mTSparam.dosymGroupIncInterval = m_iIncInterval;
  ManageSummary();
}

void CTSDoseSymDlg::OnCheckTrackBigTilt()
{
  UpdateData(true);
  mTSparam.dosymTrackBigReversal = m_bTrackOnBigTilt;
  ManageEnables();
}

void CTSDoseSymDlg::OnEnKillfocusEditBigTilt()
{
  UpdateData(true);
  mTSparam.dosymBigTiltToTrack = m_iBigTiltToTrack;
}

// Enable items depending on what is checked
void CTSDoseSymDlg::ManageEnables()
{
  m_editIncGroupAngle.EnableWindow(m_bIncGroup);
  m_statIncLabel1.EnableWindow(m_bIncGroup);
  m_statIncLabel2.EnableWindow(m_bIncGroup);
  m_statIncAmount.EnableWindow(m_bIncGroup);
  m_sbcIncAmount.EnableWindow(m_bIncGroup);
  m_statMinForAnchor.EnableWindow(m_bRunToEnd && m_bUseAnchor);
  m_statEvery.EnableWindow(m_bIncGroup);
  m_sbcMinForAnchor.EnableWindow(m_bRunToEnd && m_bUseAnchor);
  m_editRunToEnd.EnableWindow(m_bRunToEnd);
  m_butUseAnchor.EnableWindow(m_bRunToEnd);
  m_editIncInterval.EnableWindow(m_bIncGroup);
  m_statAnchorTilts.EnableWindow(m_bRunToEnd);
  EnableDlgItem(IDC_EDIT_BIG_TILT, m_bTrackOnBigTilt);
  EnableDlgItem(IDC_STAT_BIGTILT_DEG, m_bTrackOnBigTilt);
}

// Compose the summary lines
void CTSDoseSymDlg::ManageSummary()
{
  FloatVec idealAngles;
  ShortVec directions;
  int ind, final, numReversals = 0;
  float sumDiffs = 0.;
  FindDoseSymmetricAngles(mTSparam, idealAngles, directions, ind, final, 
    &m_strAngleSummary);
  for (ind = 2; ind < (int)idealAngles.size(); ind++) {
    if (!BOOL_EQUIV(idealAngles[ind] < mTSparam.bidirAngle,
      idealAngles[ind - 1] < mTSparam.bidirAngle)) {
        numReversals++;
        sumDiffs += (float)fabs(idealAngles[ind] - idealAngles[ind - 1]);
    }
  }
  m_strSummary.Format("%d tilt reversals involving %d deg total tilt", numReversals,
    B3DNINT(sumDiffs));
  UpdateData(false);
}

// Static function for working out the sequence of angles, makes optional summary line
void CTSDoseSymDlg::FindDoseSymmetricAngles(TiltSeriesParam &tsParam,
  FloatVec &idealAngles, ShortVec &directions, int &anchorIndex, int &finalPartIndex,
  CString *summary)
{
  FloatVec posAngles;
  CString str;
  bool firstDone = false, secondDone = false, addedFirst, addedSecond, uniStarted = false;
  int groupSize = tsParam.dosymBaseGroupSize;
  int ind, firstIntDir, curInd = 1, curGroup = 0, lastGroupInc = -1, lastRealGroup = -1;
  float angle, firstDir, delta, maxDelta = 0., maxFracPastEnd = 0.34f;
  float nextIncAngle = tsParam.dosymIncreaseGroups ? tsParam.dosymIncStartAngle : 10000.f;
  idealAngles.clear();
  idealAngles.push_back(tsParam.bidirAngle);

  firstIntDir = tsParam.bidirAngle > tsParam.startingTilt ? -1 : 1;
  firstDir = (float)firstIntDir;
  directions.clear();
  directions.push_back(firstIntDir);
  anchorIndex = finalPartIndex = -1;
  if (summary)
    *summary = "";

  while (!firstDone || !secondDone) {
    addedFirst = addedSecond = false;

    // Add a group of points in first direction, stop at start angle
    for (ind = 0; ind < groupSize && !firstDone; ind++) {
      delta = (float)(curInd + ind) * tsParam.tiltIncrement;
      ACCUM_MAX(maxDelta, delta + tsParam.tiltIncrement);
      angle = tsParam.bidirAngle + firstDir * delta;
      if (firstDir * angle < firstDir * tsParam.startingTilt +
        maxFracPastEnd * tsParam.tiltIncrement) {
        idealAngles.push_back(angle);
        directions.push_back(firstIntDir);
        addedFirst = true;
      } else
        firstDone = true;
    }

    // Add a group of points in second direction, stop at end angle
    for (ind = 0; ind < groupSize && !secondDone; ind++) {
      delta = (float)(curInd + ind) * tsParam.tiltIncrement;
      ACCUM_MAX(maxDelta, delta + tsParam.tiltIncrement);
      angle = tsParam.bidirAngle - firstDir * delta;
      if (-firstDir * angle < -firstDir * tsParam.endingTilt +
        maxFracPastEnd * tsParam.tiltIncrement) {
        idealAngles.push_back(angle);
        directions.push_back(-firstIntDir);
        addedSecond = true;
      } else
        secondDone = true;
    }
    
    // Manage summary
    if (summary && !str.IsEmpty() && (addedSecond || addedFirst)) {
      if (uniStarted) {
        *summary += str;
      } else if (addedSecond && addedFirst) {
          if (summary->IsEmpty())
            *summary = "Group sizes: ";
          else
            *summary += ", ";
          *summary += str;
      } else {
        str.Format(" -- unidirectional from %.1f deg", 
          (float)curInd * tsParam.tiltIncrement);
        *summary += str;
        uniStarted = true;
      }
      str = "";
    }
    curInd += groupSize;

    // Increase group number, increase size a lot to finish on one side or increase it by
    // defined increment if the angle is reached for that or the number of group steps
    // between increments has been achieved
    curGroup++;
    if ((maxDelta >= tsParam.dosymRunToEndAngle - 0.1 && tsParam.dosymDoRunToEnd) ||
      firstDone || secondDone) {
      if (groupSize < 10000) {
        lastRealGroup = groupSize;
        groupSize = 10000;
        if (!firstDone && !secondDone && tsParam.dosymAnchorIfRunToEnd)
          anchorIndex = (int)idealAngles.size() - 1;
        if (!uniStarted)
          str.Format(" -- unidirectional from %.1f deg", maxDelta);
        uniStarted = true;
      }
    } else if (maxDelta >= nextIncAngle - 0.1) {
      groupSize += tsParam.dosymGroupIncAmount;
      if (tsParam.dosymGroupIncInterval > 0)
        nextIncAngle += tsParam.dosymGroupIncInterval;
      else
        nextIncAngle = 10000.;
      str.Format("%d @ %.1f", groupSize, maxDelta);
    }
  }

  if (mIdealAnglesToUse.size() > 1 && !mIgnoreAnglesToUse)
    idealAngles = mIdealAnglesToUse;
  mIgnoreAnglesToUse = false;

  // check that there is a sign reversal after the anchorIndex if it is set; if not, unset
  // the index; if so, set the index for the place where the anchor is needed
  if (anchorIndex > 0) {
    for (ind = anchorIndex + 2; ind < (int)idealAngles.size(); ind++)
      if (!BOOL_EQUIV(idealAngles[ind] < tsParam.bidirAngle,
        idealAngles[ind - 1] < tsParam.bidirAngle)) {
        if (ind - anchorIndex <= lastRealGroup || 
          ind - anchorIndex < tsParam.dosymMinUniForAnchor)
          anchorIndex = -1;
        else
          finalPartIndex = ind;
        return;
      }
  }
  anchorIndex = -1;
}

// Given a list of angles, check its validity and set it as list to use
int CTSDoseSymDlg::SetAnglesToUse(TiltSeriesParam &tsParam, FloatVec &angles, 
  CString &mess)
{
  FloatVec idealAngles;
  ShortVec directions;
  int ind, final;
  if (angles.size() == 1) {
    CLEAR_RESIZE(mIdealAnglesToUse, float, 0);
    return 0;
  }
  mIgnoreAnglesToUse = true;
  FindDoseSymmetricAngles(tsParam, idealAngles, directions, ind, final, NULL);

  if (angles.size() != idealAngles.size()) {
    mess = "The supplied list of angles is not the same size as the original list";
    return 1;
  }
  for (ind = 0; ind < (int)angles.size(); ind++) {
    if (!BOOL_EQUIV(idealAngles[ind] < tsParam.bidirAngle,
      angles[ind] < tsParam.bidirAngle)) {
      mess.Format("Supplied angle %d (%.2f) is not on the same side of the series"
        " as the original angle (%.2f)", ind + 1, angles[ind], idealAngles[ind]);
      return 1;
    }
  }
  mIdealAnglesToUse = angles;
  return 0;
}

void CTSDoseSymDlg::IgnoreAnglesToUseNextCall()
{
  mIgnoreAnglesToUse = true;
}
