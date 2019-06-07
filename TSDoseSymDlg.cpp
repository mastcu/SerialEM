// TSDoseSymDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "TSDoseSymDlg.h"
#include "Shared\b3dutil.h"


// CTSDoseSymDlg dialog

CTSDoseSymDlg::CTSDoseSymDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_TSDOSESYM, pParent)
  , m_strGroupSize(_T(""))
  , m_bIncGroup(FALSE)
  , m_fIncGroupAngle(0)
  , m_strIncAmount(_T(""))
  , m_strIncInterval(_T(""))
  , m_bRunToEnd(FALSE)
  , m_fRunToEnd(0)
  , m_strSummary(_T(""))
  , m_bUseAnchor(FALSE)
  , m_iMinForAnchor(0)
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
  DDX_Control(pDX, IDC_STAT_INC_INTERVAL, m_statIncInterval);
  DDX_Text(pDX, IDC_STAT_INC_INTERVAL, m_strIncInterval);
  DDX_Control(pDX, IDC_SPIN_INC_INTERVAL, m_sbcIncInterval);
  DDX_Check(pDX, IDC_CHECK_RUN_TO_END, m_bRunToEnd);
  DDX_Control(pDX, IDC_EDIT_RUN_TO_END, m_editRunToEnd);
  DDX_Text(pDX, IDC_EDIT_RUN_TO_END, m_fRunToEnd);
  DDV_MinMaxFloat(pDX, m_fRunToEnd, 0, 90);
  DDX_Text(pDX, IDC_STAT_SUMMARY, m_strSummary);
  DDX_Control(pDX, IDC_CHECK_USE_ANCHOR, m_butUseAnchor);
  DDX_Check(pDX, IDC_CHECK_USE_ANCHOR, m_bUseAnchor);
  DDX_Control(pDX, IDC_EDIT_MIN_FOR_ANCHOR, m_editMinForAnchor);
  DDX_Text(pDX, IDC_EDIT_MIN_FOR_ANCHOR, m_iMinForAnchor);
  DDV_MinMaxInt(pDX, m_iMinForAnchor, 2, 20);
  DDX_Control(pDX, IDC_STAT_ANCHOR_TILTS, m_statAnchorTilts);
}


BEGIN_MESSAGE_MAP(CTSDoseSymDlg, CBaseDlg)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_GROUP_SIZE, OnDeltaposSpinGroupSize)
  ON_BN_CLICKED(IDC_CHECK_INC_GROUP, OnCheckIncGroup)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_INC_AMOUNT, OnDeltaposSpinIncAmount)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_INC_INTERVAL, OnDeltaposSpinIncInterval)
  ON_BN_CLICKED(IDC_CHECK_RUN_TO_END, OnCheckRunToEnd)
  ON_EN_KILLFOCUS(IDC_EDIT_INC_GROUP_ANGLE, OnEnKillfocusEditIncGroupAngle)
  ON_EN_KILLFOCUS(IDC_EDIT_RUN_TO_END, OnEnKillfocusEditRunToEnd)
  ON_BN_CLICKED(IDC_CHECK_USE_ANCHOR, OnCheckUseAnchor)
  ON_EN_KILLFOCUS(IDC_EDIT_MIN_FOR_ANCHOR, &CTSDoseSymDlg::OnEnKillfocusEditMinForAnchor)
END_MESSAGE_MAP()


// CTSDoseSymDlg message handlers
BOOL CTSDoseSymDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  m_bIncGroup = mTSparam.dosymIncreaseGroups;
  m_bRunToEnd = mTSparam.dosymDoRunToEnd;
  m_fRunToEnd = mTSparam.dosymRunToEndAngle;
  m_fIncGroupAngle = mTSparam.dosymIncStartAngle;
  m_bUseAnchor = mTSparam.dosymAnchorIfRunToEnd;
  m_iMinForAnchor = mTSparam.dosymMinUniForAnchor;
  m_strGroupSize.Format("%d", mTSparam.dosymBaseGroupSize);
  m_strIncAmount.Format("%d", mTSparam.dosymGroupIncInterval);
  m_strIncInterval.Format("every %d", mTSparam.dosymGroupIncInterval);
  m_sbcGroupSize.SetRange(0, 1000);
  m_sbcGroupSize.SetPos(500);
  m_sbcIncAmount.SetRange(0, 1000);
  m_sbcIncAmount.SetPos(500);
  m_sbcIncInterval.SetRange(0, 1000);
  m_sbcIncInterval.SetPos(500);
  ManageEnables();
  ManageSummary();
  return TRUE;
}

void CTSDoseSymDlg::OnOK()
{
  SetFocus();
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

void CTSDoseSymDlg::OnDeltaposSpinIncInterval(NMHDR *pNMHDR, LRESULT *pResult)
{
  FormattedSpinnerValue(pNMHDR, pResult, 1, 20, mTSparam.dosymGroupIncInterval,
    m_strIncInterval, "every %d");
  *pResult = 0;
  ManageSummary();
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
}

void CTSDoseSymDlg::OnEnKillfocusEditMinForAnchor()
{
  UpdateData(true);
  mTSparam.dosymMinUniForAnchor = m_iMinForAnchor;
}

void CTSDoseSymDlg::ManageEnables()
{
  m_editIncGroupAngle.EnableWindow(m_bIncGroup);
  m_statIncLabel1.EnableWindow(m_bIncGroup);
  m_statIncLabel2.EnableWindow(m_bIncGroup);
  m_statIncAmount.EnableWindow(m_bIncGroup);
  m_sbcIncAmount.EnableWindow(m_bIncGroup);
  m_statIncInterval.EnableWindow(m_bIncGroup);
  m_sbcIncInterval.EnableWindow(m_bIncGroup);
  m_editRunToEnd.EnableWindow(m_bRunToEnd);
  m_butUseAnchor.EnableWindow(m_bRunToEnd);
  m_editMinForAnchor.EnableWindow(m_bRunToEnd && m_bUseAnchor);
  m_statAnchorTilts.EnableWindow(m_bRunToEnd);
}

void CTSDoseSymDlg::ManageSummary()
{
  FloatVec idealAngles;
  ShortVec directions;
  int ind, final, numReversals = 0;
  float sumDiffs = 0.;
  FindDoseSymmetricAngles(mTSparam, idealAngles, directions, ind, final);
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

// Static function for working out the sequence of angles
void CTSDoseSymDlg::FindDoseSymmetricAngles(TiltSeriesParam &tsParam,
  FloatVec &idealAngles, ShortVec &directions, int &anchorIndex, int &finalPartIndex)
{
  FloatVec posAngles;
  bool firstDone = false, secondDone = false;
  int groupSize = tsParam.dosymBaseGroupSize;
  int ind, firstIntDir, curInd = 1, curGroup = 0, lastGroupInc = -1, lastRealGroup = -1;
  float angle, firstDir, delta, maxDelta = 0., maxFracPastEnd = 0.34f;
  idealAngles.clear();
  idealAngles.push_back(tsParam.bidirAngle);

  firstIntDir = tsParam.bidirAngle > tsParam.startingTilt ? -1 : 1;
  firstDir = (float)firstIntDir;
  directions.push_back(firstIntDir);
  anchorIndex = finalPartIndex = -1;
  while (!firstDone || !secondDone) {

    // Add a group of points in first direction, stop at start angle
    for (ind = 0; ind < groupSize && !firstDone; ind++) {
      delta = (float)(curInd + ind) * tsParam.tiltIncrement;
      ACCUM_MAX(maxDelta, delta + tsParam.tiltIncrement);
      angle = tsParam.bidirAngle + firstDir * delta;
      if (firstDir * angle < firstDir * tsParam.startingTilt +
        maxFracPastEnd * tsParam.tiltIncrement) {
        idealAngles.push_back(angle);
        directions.push_back(firstIntDir);
      } else
        firstDone = true;
    }

    // Add a group of points in second direction, stop at end angle
    for (ind = 0; ind < groupSize && !secondDone; ind++) {
      delta = (float)(curInd + ind) * tsParam.tiltIncrement;
      ACCUM_MAX(maxDelta, delta);
      angle = tsParam.bidirAngle - firstDir * delta;
      if (-firstDir * angle < -firstDir * tsParam.endingTilt +
        maxFracPastEnd * tsParam.tiltIncrement) {
        idealAngles.push_back(angle);
        directions.push_back(-firstIntDir);
      } else
        secondDone = true;
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
        if (!firstDone && !secondDone)
          anchorIndex = (int)idealAngles.size();
      }
    } else if (maxDelta >= tsParam.dosymIncStartAngle - 0.1 && 
      tsParam.dosymIncreaseGroups) {
      if (lastGroupInc < 0 || curGroup - lastGroupInc >= tsParam.dosymGroupIncInterval) {
        groupSize += tsParam.dosymGroupIncAmount;
        lastGroupInc = curGroup;
      }
    }
  }

  // check that there is a sign reversal after the anchorIndex if it is set
  if (anchorIndex > 0) {
    for (ind = anchorIndex + 1; ind < (int)idealAngles.size(); ind++)
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
