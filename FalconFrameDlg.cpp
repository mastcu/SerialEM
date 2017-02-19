// FalconFrameDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "FalconFrameDlg.h"
#include "ParameterIO.h"
#include "FalconHelper.h"
#include "CameraController.h"
#include "MacroProcessor.h"


// CFalconFrameDlg dialog

CFalconFrameDlg::CFalconFrameDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CFalconFrameDlg::IDD, pParent)
  , m_fExposure(0)
  , m_strReadouts(_T(""))
  , m_strSkipStart(_T(""))
  , m_strNumFrames(_T(""))
  , m_strTotalSave(_T(""))
  , m_strSkipEnd(_T(""))
  , m_fSubframeTime(0)
{

}

CFalconFrameDlg::~CFalconFrameDlg()
{
}

void CFalconFrameDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_EDIT_EXPOSURE, m_fExposure);
  DDX_Text(pDX, IDC_EDIT_READOUTS, m_strReadouts);
  DDX_Control(pDX, IDC_SPIN_NUM_FRAMES, m_sbcNumFrames);
  DDX_Control(pDX, IDC_SPIN_SKIP_START, m_sbcSkipStart);
  DDX_Control(pDX, IDC_SPIN_TOTAL_SAVE, m_sbcTotalSave);
  DDX_Control(pDX, IDC_SPIN_SKIP_AFTER, m_sbcSkipAfter);
  DDX_Text(pDX, IDC_STAT_SKIP_START, m_strSkipStart);
  DDX_Text(pDX, IDC_STAT_NUM_FRAMES, m_strNumFrames);
  DDX_Text(pDX, IDC_STAT_TOTAL_SAVE, m_strTotalSave);
  DDX_Text(pDX, IDC_STAT_SKIP_END, m_strSkipEnd);
  DDX_Control(pDX, IDC_STAT_STARTLABEL, m_statStartLabel);
  DDX_Control(pDX, IDC_STAT_ENDLABEL, m_statEndLabel);
  DDX_Control(pDX, IDC_EDIT_SUBFRAME_TIME, m_editSubframeTime);
  DDX_Text(pDX, IDC_EDIT_SUBFRAME_TIME, m_fSubframeTime);
  DDX_Control(pDX, IDC_STAT_SKIP_START, m_statSkipStart);
  DDX_Control(pDX, IDC_STAT_SKIP_END, m_statSkipEnd);
  DDX_Control(pDX, IDC_STAT_EXPLANATION, m_statExplanation);
}


BEGIN_MESSAGE_MAP(CFalconFrameDlg, CBaseDlg)
  ON_EN_KILLFOCUS(IDC_EDIT_EXPOSURE, OnKillfocusEditExposure)
  ON_EN_KILLFOCUS(IDC_EDIT_READOUTS, OnKillfocusEditReadouts)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_NUM_FRAMES, OnDeltaposSpinNumFrames)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_SKIP_START, OnDeltaposSpinSkipStart)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_TOTAL_SAVE, OnDeltaposSpinTotalSave)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_SKIP_AFTER, OnDeltaposSpinSkipAfter)
  ON_EN_KILLFOCUS(IDC_EDIT_SUBFRAME_TIME, OnKillfocusSubframeTime)
  ON_BN_CLICKED(IDC_BUT_EQUALIZE, OnButEqualize)
END_MESSAGE_MAP()


// CFalconFrameDlg message handlers
BOOL CFalconFrameDlg::OnInitDialog()
{
  int show = mK2Type ? SW_HIDE : SW_SHOW;
  CBaseDlg::OnInitDialog();
  mHelper = mWinApp->mFalconHelper;
  m_sbcNumFrames.SetRange(0, 30000);
  m_sbcNumFrames.SetPos(15000);
  m_sbcTotalSave.SetRange(0, 30000);
  m_sbcTotalSave.SetPos(15000);
  m_sbcSkipStart.SetRange(0, 30000);
  m_sbcSkipStart.SetPos(15000);
  m_sbcSkipAfter.SetRange(0, 30000);
  m_sbcSkipAfter.SetPos(15000);
  m_editSubframeTime.EnableWindow(mK2Type);
  m_statStartLabel.ShowWindow(show);
  m_statEndLabel.ShowWindow(show);
  m_sbcSkipAfter.ShowWindow(show);
  m_sbcSkipStart.ShowWindow(show);
  m_statSkipStart.ShowWindow(show);
  m_statSkipEnd.ShowWindow(show);
  m_statExplanation.ShowWindow(mK2Type ? SW_SHOW : SW_HIDE);

  if (!mSummedFrameList.size()) {
    mSummedFrameList.push_back(1);
    mSummedFrameList.push_back(B3DNINT(B3DMAX(1., mExposure / mReadoutInterval)));
    OnButEqualize();
  } else if (!mUserFrameFrac.size() || !mUserSubframeFrac.size()) {
    OnButEqualize();
  } else {
    UpdateAllDisplays();
  }
  SetDefID(45678);    // Disable OK from being default button
  return TRUE;
}
void CFalconFrameDlg::OnOK() 
{
  SetFocus();
	CBaseDlg::OnOK();
}

void CFalconFrameDlg::OnCancel() 
{
  CBaseDlg::OnCancel();
}

// A new exposure time
void CFalconFrameDlg::OnKillfocusEditExposure()
{
  UpdateData(TRUE);
  mHelper->AdjustForExposure(mSummedFrameList, mNumSkipBefore, mNumSkipAfter,
    m_fExposure, mReadoutInterval, mUserFrameFrac, mUserSubframeFrac);
  UpdateAllDisplays();
}

void CFalconFrameDlg::OnKillfocusSubframeTime()
{
  UpdateData(TRUE);
  mWinApp->mCamera->ConstrainFrameTime(m_fSubframeTime);
  mReadoutInterval = m_fSubframeTime;
  mHelper->AdjustForExposure(mSummedFrameList, mNumSkipBefore, mNumSkipAfter,
    m_fExposure, mReadoutInterval, mUserFrameFrac, mUserSubframeFrac);
  UpdateAllDisplays();
}

// A new entry in the edit box with all the frames
void CFalconFrameDlg::OnKillfocusEditReadouts()
{
  int ind, totFrames, totSubframes, numVals, currentIndex = 0;
  int lineList[2];
  bool changed, badEntry = false;
  CString strLine;
  ShortVec newList;
  UpdateData(TRUE);
  while (currentIndex < m_strReadouts.GetLength()) {
    mWinApp->mMacroProcessor->GetNextLine(&m_strReadouts, currentIndex, strLine);
    mWinApp->mParamIO->StringToEntryList(3, strLine, numVals, lineList, NULL, 2);
    if (!numVals)
      continue;
    if (numVals < 2 || lineList[0] <= 0 || lineList[0] > 30000 || lineList[1] <= 0 ||
      lineList[1] > 30000) {
        badEntry = true;
        break;
    }
    newList.push_back(lineList[0]);
    newList.push_back(lineList[1]);
  }
  if (!badEntry && newList.size() > 0) {

    // If list is valid, see if it has changed
    changed = newList.size() != mSummedFrameList.size();
    for (ind = 0; ind < (int)newList.size() && !changed; ind++)
      changed = newList[ind] != mSummedFrameList[ind];

    // If it changed, record new fractions for user entry
    if (changed) {
      totFrames = mHelper->GetFrameTotals(newList, totSubframes);
      mUserFrameFrac.resize(newList.size() / 2);
      mUserSubframeFrac.resize(newList.size() / 2);
      for (ind = 0; ind < (int)newList.size() / 2; ind++) {
        mUserFrameFrac[ind] = (float)newList[ind * 2] / (float)totFrames;
        mUserSubframeFrac[ind] = ((float)(newList[ind * 2] * newList[ind * 2 + 1])) /
          (float)totSubframes;
      }
      mSummedFrameList = newList;
    }
  }
    
  UpdateAllDisplays();
}


// A new number of frames
void CFalconFrameDlg::OnDeltaposSpinNumFrames(NMHDR *pNMHDR, LRESULT *pResult)
{
  int newval, newTotal, totSub, totFrames;
  totFrames = mHelper->GetFrameTotals(mSummedFrameList, totSub);
  if (NewSpinnerValue(pNMHDR, pResult, totFrames, 1, mMaxFrames, newval))
    return;
  newTotal = B3DMIN(mMaxFrames * mMaxPerFrame, B3DMAX(newval, totSub));
  mHelper->DistributeSubframes(mSummedFrameList, newTotal, newval, mUserFrameFrac,
    mUserSubframeFrac);
  UpdateAllDisplays();
}

// Change in skip at start
void CFalconFrameDlg::OnDeltaposSpinSkipStart(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, mNumSkipBefore, 0, mMaxFrames,
    mNumSkipBefore))
    return;
  UpdateAllDisplays();
}

// A new number of subframes
void CFalconFrameDlg::OnDeltaposSpinTotalSave(NMHDR *pNMHDR, LRESULT *pResult)
{
  int newval, totSub, totFrames;
  totFrames = mHelper->GetFrameTotals(mSummedFrameList, totSub);
  if (NewSpinnerValue(pNMHDR, pResult, totSub, 1, mMaxFrames * mMaxPerFrame, newval))
    return;
  totFrames = B3DMIN(newval, totFrames);
  mHelper->DistributeSubframes(mSummedFrameList, newval, totFrames, mUserFrameFrac,
    mUserSubframeFrac);
  UpdateAllDisplays();
}

// Change in skip at end
void CFalconFrameDlg::OnDeltaposSpinSkipAfter(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, mNumSkipAfter, 0, mMaxFrames,
    mNumSkipAfter))
    return;
  UpdateAllDisplays();
}

// Set to one block with equal frame sizes
void CFalconFrameDlg::OnButEqualize()
{
  int totSub, totFrames;
  totFrames = mHelper->GetFrameTotals(mSummedFrameList, totSub);
  mUserFrameFrac.resize(1);
  mUserFrameFrac[0] = 1.;
  mUserSubframeFrac.resize(1);
  mUserSubframeFrac[0] = 1.;
  mHelper->DistributeSubframes(mSummedFrameList, totSub, totFrames, mUserFrameFrac,
    mUserSubframeFrac);
  UpdateAllDisplays();
}

// Add up the subframes in the list and set all elements of display based on that
void CFalconFrameDlg::UpdateAllDisplays(void)
{
  int i, numFrames = mHelper->GetFrameTotals(mSummedFrameList, mTotalSaved);
  CString oneval;
  m_strReadouts = "";
  for (i = 0; i < (int)mSummedFrameList.size() / 2; i++) {
    oneval.Format("%d              %d\r\n", mSummedFrameList[2 * i], 
      mSummedFrameList[2 * i + 1]);
    m_strReadouts += oneval;
  }
  m_strNumFrames.Format("%d", numFrames);
  m_strSkipStart.Format("%d", mNumSkipBefore);
  m_strSkipEnd.Format("%d", mNumSkipAfter);
  m_strTotalSave.Format("%d", mTotalSaved);
  mExposure = (mTotalSaved + mNumSkipBefore + mNumSkipAfter) * mReadoutInterval;
  m_fExposure = (float)(B3DNINT(mExposure * 200.) / 200.);
  UpdateData(false);
}


