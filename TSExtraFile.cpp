// TSExtraFile.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include ".\TSExtraFile.h"
#include "TiltSeriesParam.h"
#include "ParameterIO.h"
#include "EMbufferManager.h"
#include "EMscope.h"
#include "CameraController.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTSExtraFile dialog


CTSExtraFile::CTSExtraFile(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CTSExtraFile::IDD, pParent)
  , m_strKBpImage(_T(""))
  , m_bStackRecord(FALSE)
  , m_bSetSpot(FALSE)
  , m_bSetExposure(FALSE)
  , m_strNewSpot(_T(""))
  , m_fDeltaC2(0)
  , m_fNewDrift(0)
  , m_bTrialBin(FALSE)
  , m_strTrialBin(_T(""))
  , m_strExposures(_T(""))
  , m_bKeepBidirAnchor(FALSE)
{
	//{{AFX_DATA_INIT(CTSExtraFile)
	m_bSaveView = FALSE;
	m_bSaveFocus = FALSE;
	m_bSavePreview = FALSE;
	m_bSaveTrial = FALSE;
	m_strExtraRec = _T("");
	m_iWhichRecord = -1;
	m_strInstruct1 = _T("");
	m_strInstruct2 = _T("");
	m_strEntries = _T("");
	//}}AFX_DATA_INIT
}


void CTSExtraFile::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CTSExtraFile)
  DDX_Control(pDX, IDC_EDITENTRIES, m_editEntries);
  DDX_Control(pDX, IDC_SAVETRIAL, m_butTrial);
  DDX_Control(pDX, IDC_SAVEPREVIEW, m_butPreview);
  DDX_Control(pDX, IDC_SAVEFOCUS, m_butFocus);
  DDX_Control(pDX, IDC_TSSAVEVIEW, m_butView);
  DDX_Control(pDX, IDC_SPINVIEW, m_sbcView);
  DDX_Control(pDX, IDC_SPINTRIAL, m_sbcTrial);
  DDX_Control(pDX, IDC_SPINRECORD, m_sbcRecord);
  DDX_Control(pDX, IDC_SPINPREVIEW, m_sbcPreview);
  DDX_Control(pDX, IDC_SPINFOCUS, m_sbcFocus);
  DDX_Check(pDX, IDC_TSSAVEVIEW, m_bSaveView);
  DDX_Check(pDX, IDC_SAVEFOCUS, m_bSaveFocus);
  DDX_Check(pDX, IDC_SAVEPREVIEW, m_bSavePreview);
  DDX_Check(pDX, IDC_SAVETRIAL, m_bSaveTrial);
  DDX_Text(pDX, IDC_STATEXTRAREC, m_strExtraRec);
  DDX_Radio(pDX, IDC_NOEXTRA, m_iWhichRecord);
  DDX_Text(pDX, IDC_STATINSTRUCT1, m_strInstruct1);
  DDX_Text(pDX, IDC_STATINSTRUCT2, m_strInstruct2);
  DDX_Text(pDX, IDC_EDITENTRIES, m_strEntries);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_SPINBINSIZE, m_sbcBinSize);
  DDX_Text(pDX, IDC_STATKBPIMAGE, m_strKBpImage);
  DDX_Check(pDX, IDC_STACKRECORD, m_bStackRecord);
  DDX_Check(pDX, IDC_CHECK_SETSPOT, m_bSetSpot);
  DDX_Check(pDX, IDC_CHECK_SETEXPOSURE, m_bSetExposure);
  DDX_Control(pDX, IDC_STAT_NEWSPOT, m_statNewSpot);
  DDX_Text(pDX, IDC_STAT_NEWSPOT, m_strNewSpot);
  DDX_Control(pDX, IDC_SPIN_NEWSPOT, m_sbcNewSpot);
  DDX_Control(pDX, IDC_EDIT_NEWEXPOSURE, m_editNewExposure);
  DDX_Control(pDX, IDC_EDIT_NEWDRIFT, m_editNewDrift);
  DDX_Control(pDX, IDC_EDIT_DELTAC2, m_editDeltaC2);
  DDX_Text(pDX, IDC_EDIT_DELTAC2, m_fDeltaC2);
  DDV_MinMaxFloat(pDX, m_fDeltaC2, 0.001f, 1000.f);
  DDX_Text(pDX, IDC_EDIT_NEWDRIFT, m_fNewDrift);
  DDV_MinMaxFloat(pDX, m_fNewDrift, 0.0, 10.f);
  DDX_Control(pDX, IDC_OPPOSITETRIAL, m_butOppositeTrial);
  DDX_Control(pDX, IDC_CHECK_TRIAL_BIN, m_butTrialBin);
  DDX_Control(pDX, IDC_STAT_TRIAL_BIN, m_statTrialBin);
  DDX_Control(pDX, IDC_SPIN_TRIAL_BIN, m_sbcTrialBin);
  DDX_Check(pDX, IDC_CHECK_TRIAL_BIN, m_bTrialBin);
  DDX_Text(pDX, IDC_STAT_TRIAL_BIN, m_strTrialBin);
  DDX_Control(pDX, IDC_CHECK_SETSPOT, m_butSetSpot);
  DDX_Text(pDX, IDC_EDIT_NEWEXPOSURE, m_strExposures);
  DDX_Check(pDX, IDC_CHECK_KEEP_ANCHOR, m_bKeepBidirAnchor);
}


BEGIN_MESSAGE_MAP(CTSExtraFile, CBaseDlg)
	//{{AFX_MSG_MAP(CTSExtraFile)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPINFOCUS, OnDeltaposSpinfocus)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPINPREVIEW, OnDeltaposSpinpreview)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPINRECORD, OnDeltaposSpinrecord)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPINTRIAL, OnDeltaposSpintrial)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPINVIEW, OnDeltaposSpinview)
	ON_BN_CLICKED(IDC_NOEXTRA, OnRecordRadio)
	ON_BN_CLICKED(IDC_FOCUSSERIES, OnRecordRadio)
	ON_BN_CLICKED(IDC_FILTERSERIES, OnRecordRadio)
	ON_BN_CLICKED(IDC_OPPOSITETRIAL, OnRecordRadio)
	ON_BN_CLICKED(IDC_OTHERCHANNELS, OnRecordRadio)
	//}}AFX_MSG_MAP
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINBINSIZE, OnDeltaposSpinbinsize)
  ON_BN_CLICKED(IDC_CHECK_SETSPOT, OnCheckSetspot)
  ON_BN_CLICKED(IDC_CHECK_SETEXPOSURE, OnCheckSetexposure)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_NEWSPOT, OnDeltaposSpinNewspot)
  ON_BN_CLICKED(IDC_CHECK_TRIAL_BIN, OnCheckTrialBin)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_TRIAL_BIN, OnDeltaposSpinTrialBin)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTSExtraFile message handlers

void CTSExtraFile::OnDeltaposSpinfocus(NMHDR* pNMHDR, LRESULT* pResult) 
{
  DeltaposSpin(1, IDC_STATFOCUSNUM, pNMHDR, pResult);
}

void CTSExtraFile::OnDeltaposSpinpreview(NMHDR* pNMHDR, LRESULT* pResult) 
{
  DeltaposSpin(4, IDC_STATPREVIEWNUM, pNMHDR, pResult);
}

void CTSExtraFile::OnDeltaposSpinrecord(NMHDR* pNMHDR, LRESULT* pResult) 
{
  DeltaposSpin(3, IDC_STATRECORDNUM, pNMHDR, pResult);
}

void CTSExtraFile::OnDeltaposSpintrial(NMHDR* pNMHDR, LRESULT* pResult) 
{
  DeltaposSpin(2, IDC_STATTRIALNUM, pNMHDR, pResult);
}

void CTSExtraFile::OnDeltaposSpinview(NMHDR* pNMHDR, LRESULT* pResult) 
{
  DeltaposSpin(0, IDC_STATVIEWNUM, pNMHDR, pResult);
}

void CTSExtraFile::DeltaposSpin(int index, int statID, NMHDR *pNMHDR, LRESULT *pResult)
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  int newVal = pNMUpDown->iPos + pNMUpDown->iDelta;
  if (newVal < 0 || newVal >= MAX_STORES) {
    *pResult = 1;
    return;
  }
  mFileIndex[index] = newVal;
  FormatNumAndName(newVal, statID);
	*pResult = 0;
}


void CTSExtraFile::OnDeltaposSpinbinsize(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  *pResult = 1;
  int newVal = mMaxStackSizeXY + pNMUpDown->iDelta * 32;
  if (newVal < 128 || newVal > mWinApp->mBufferManager->GetStackWinMaxXY())
    return;
  UpdateData(true);
  mMaxStackSizeXY = newVal;
  m_strKBpImage.Format("%d KB/image (e.g., %d x %d)", 
    mMaxStackSizeXY * mMaxStackSizeXY / 1024, mMaxStackSizeXY, mMaxStackSizeXY);
  UpdateData(false);
	*pResult = 0;
}

void CTSExtraFile::OnCheckSetspot()
{
  UpdateData(true);
  m_sbcNewSpot.EnableWindow(m_bSetSpot);
  m_statNewSpot.EnableWindow(m_bSetSpot);
}

void CTSExtraFile::OnCheckSetexposure()
{
  UpdateData(true);
  m_editNewExposure.EnableWindow(m_bSetExposure);
  m_editNewDrift.EnableWindow(m_bSetExposure && m_iWhichRecord != TS_OTHER_CHANNELS);
}

void CTSExtraFile::OnDeltaposSpinNewspot(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  *pResult = 1;
  int newVal = mSpotSize + pNMUpDown->iDelta;
  if (newVal < 1 || newVal > mNumSpotSizes)
    return;
  mSpotSize = newVal;
  m_strNewSpot.Format("%d", mSpotSize);
  UpdateData(false);
  *pResult = 0;
}

void CTSExtraFile::OnCheckTrialBin()
{
  UpdateData(true);
  ManageExtraRecords();
}

void CTSExtraFile::OnDeltaposSpinTrialBin(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  *pResult = 1;
  int newVal = mBinIndex + pNMUpDown->iDelta;
  if (newVal < mCamParam->K2Type ? 1 : 0 || newVal >= mCamParam->numBinnings)
    return;
  mBinIndex = newVal;
  m_strTrialBin.Format("%s", (LPCTSTR)mWinApp->BinningText(
    mCamParam->binnings[newVal], mCamParam->K2Type ? 2 : 1));
  UpdateData(false);
  *pResult = 0;
}

// Make sure the text string is good before closing the dialog
void CTSExtraFile::OnOK() 
{
  UpdateData(true);
  if (ValidateRecordEntries(m_iWhichRecord) > 0)
    return;
  if (m_strExposures.IsEmpty()) {
    mNumExtraExposures = 0;
  } else if (mWinApp->mParamIO->StringToEntryList(1, m_strExposures, mNumExtraExposures, 
    NULL, &mExtraExposures[0], MAX_EXTRA_RECORDS)) {
      AfxMessageBox("There is a problem with the exposure time entry.\n"
        "It should consist of one or more numbers separated by spaces, not commas.\n\n"
        "You need to fix the entry or set it to 0.");
      return;
  }

  mSaveOn[0] = m_bSaveView;
  mSaveOn[1] = m_bSaveFocus;
  mSaveOn[2] = m_bSaveTrial;
  mSaveOn[4] = m_bSavePreview;
	CBaseDlg::OnOK();
}

BOOL CTSExtraFile::OnInitDialog() 
{
	CBaseDlg::OnInitDialog();
  CString *modeNames = mWinApp->GetModeNames();
  mCamParam = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  m_sbcBinSize.SetRange(0, 100);
  m_sbcBinSize.SetPos(50);
  if (mMaxStackSizeXY > mWinApp->mBufferManager->GetStackWinMaxXY())
    mMaxStackSizeXY = mWinApp->mBufferManager->GetStackWinMaxXY();
  m_strKBpImage.Format("%d KB/image (e.g., %d x %d)", 
    mMaxStackSizeXY * mMaxStackSizeXY / 1024, mMaxStackSizeXY, mMaxStackSizeXY);
  m_butView.SetWindowText(modeNames[0]);
  m_butFocus.SetWindowText(modeNames[1]);
  m_butTrial.SetWindowText(modeNames[2]);
  m_butPreview.SetWindowText(modeNames[4]);
  ReplaceDlgItemText(IDC_STACKRECORD, "Record", modeNames[3]);
  ReplaceDlgItemText(IDC_GRPEXTRAREC, "Record", modeNames[3]);
  ReplaceDlgItemText(IDC_GRPEXTRAREC, "Trial", modeNames[2]);
  ReplaceDlgItemText(IDC_GRPCHANGES, "Record", modeNames[3]);
  ReplaceDlgItemText(IDC_GRPCHANGES, "Trial", modeNames[2]);
  ReplaceDlgItemText(IDC_OPPOSITETRIAL, "Trial", modeNames[2]);
  ReplaceDlgItemText(IDC_CHECK_TRIAL_BIN, "Trial", modeNames[2]);
  ReplaceDlgItemText(IDC_STATEXTRAINSTR1, "Record", modeNames[3]);
  m_bSaveView = mSaveOn[0];	
  m_bSaveFocus = mSaveOn[1];	
  m_bSaveTrial = mSaveOn[2];	
  m_bSavePreview = mSaveOn[4];
  m_sbcView.SetRange(0, MAX_STORES - 1);
  m_sbcFocus.SetRange(0, MAX_STORES - 1);
  m_sbcTrial.SetRange(0, MAX_STORES - 1);
  m_sbcRecord.SetRange(0, MAX_STORES - 1);
  m_sbcPreview.SetRange(0, MAX_STORES - 1);
  m_sbcView.SetPos(mFileIndex[0]);
  m_sbcFocus.SetPos(mFileIndex[1]);
  m_sbcTrial.SetPos(mFileIndex[2]);
  m_sbcRecord.SetPos(mFileIndex[3]);
  m_sbcPreview.SetPos(mFileIndex[4]);
  FormatNumAndName(mFileIndex[0], IDC_STATVIEWNUM);
  FormatNumAndName(mFileIndex[1], IDC_STATFOCUSNUM);
  FormatNumAndName(mFileIndex[2], IDC_STATTRIALNUM);
  FormatNumAndName(mFileIndex[3], IDC_STATRECORDNUM);
  FormatNumAndName(mFileIndex[4], IDC_STATPREVIEWNUM);
  m_sbcNewSpot.EnableWindow(m_bSetSpot);
  m_statNewSpot.EnableWindow(m_bSetSpot);
  m_editNewExposure.EnableWindow(m_bSetExposure);
  m_strExposures = mWinApp->mParamIO->EntryListToString(1, 2, mNumExtraExposures, NULL, 
    &mExtraExposures[0]);
  mNumSpotSizes = mWinApp->mScope->GetNumSpotSizes();
  m_sbcNewSpot.SetRange(1, mNumSpotSizes);
  m_sbcNewSpot.SetPos(mSpotSize);
  m_strNewSpot.Format("%d", mSpotSize);
  m_sbcTrialBin.SetRange(1, 50);
  m_sbcTrialBin.SetPos(25);
  mBinIndex = B3DMIN(mBinIndex, mCamParam->numBinnings - 1);
  if (mCamParam->K2Type)
    mBinIndex = B3DMAX(1, mBinIndex);
  m_strTrialBin.Format("%s", (LPCTSTR)mWinApp->BinningText(
    mCamParam->binnings[mBinIndex], mCamParam->K2Type ? 2 : 1));
  if (!mWinApp->LowDoseMode() || mWinApp->mLowDoseDlg.ShiftsBalanced()) {
    m_butOppositeTrial.EnableWindow(false);
    if (m_iWhichRecord == TS_OPPOSITE_TRIAL)
      m_iWhichRecord = NO_TS_EXTRA_RECORD;
  }

  if (!mWinApp->ScopeHasFilter() || (mCamParam->STEMcamera && mCamParam->numChannels > 1))
  {
    CButton *radio = (CButton *)GetDlgItem(IDC_FILTERSERIES);
    radio->EnableWindow(false);
    if (m_iWhichRecord == TS_FILTER_SERIES)
      m_iWhichRecord = NO_TS_EXTRA_RECORD;
  }

  if (mCamParam->STEMcamera && mCamParam->numChannels > 1) {
    CButton *radio = (CButton *)GetDlgItem(IDC_FOCUSSERIES);
    radio->EnableWindow(false);
    if (m_iWhichRecord == TS_FOCUS_SERIES)
      m_iWhichRecord = NO_TS_EXTRA_RECORD;
  } else {
    CButton *radio = (CButton *)GetDlgItem(IDC_OTHERCHANNELS);
    radio->EnableWindow(false);
    if (m_iWhichRecord == TS_OTHER_CHANNELS)
      m_iWhichRecord = NO_TS_EXTRA_RECORD;
  }
  if (mCamParam->STEMcamera && mCamParam->FEItype)
    mWinApp->mScope->GetFEIChannelList(mCamParam, true);
  m_editNewDrift.EnableWindow(m_bSetExposure && m_iWhichRecord != TS_OTHER_CHANNELS);
  ManageExtraRecords();
	return TRUE;
}

// Take care of file number and name on one of the spin buttons
void CTSExtraFile::FormatNumAndName(int index, int statID)
{
  CStatic *stat = (CStatic *)GetDlgItem(statID);
  CString str, name;
  KImageStore *store = ((CSerialEMApp *)AfxGetApp())->mDocWnd->GetStoreMRC(index);
  int i;
  if (store) {
    name = store->getName();
    i = name.ReverseFind('\\');
    if (i >= 0)
      name = name.Right(name.GetLength() - i - 1);
  } else
    name = "Not open yet";

  str.Format("%d: %s", index + 1, (LPCTSTR)name);
	stat->SetWindowText(str);
}

// Respond to the extra record selection: validate before changing
void CTSExtraFile::OnRecordRadio() 
{
  int oldWhich = m_iWhichRecord;
  UpdateData(true);
  if (ValidateRecordEntries(oldWhich) > 0) {
    m_iWhichRecord = oldWhich;
    UpdateData(false);
    return;
  }
  ManageExtraRecords();
}

// For a given selection of which extra record, manage the instructions and put the list
// in the edit box
void CTSExtraFile::ManageExtraRecords()
{
  CString *modeNames = mWinApp->GetModeNames();
  CString str;
  int numSimul, numAvail, simultaneous[3];
  bool trials = m_iWhichRecord == TS_OPPOSITE_TRIAL;
  bool STEM = m_iWhichRecord == TS_OTHER_CHANNELS;
  m_strExtraRec.Format("Extra %ss", modeNames[trials ? 2 : 3]);
  m_editEntries.EnableWindow(m_iWhichRecord != NO_TS_EXTRA_RECORD && !trials);
  m_butTrialBin.EnableWindow(trials);
  m_sbcTrialBin.EnableWindow(trials && m_bTrialBin);
  m_statTrialBin.EnableWindow(trials && m_bTrialBin);
  m_editDeltaC2.EnableWindow(!STEM);
  m_editNewDrift.EnableWindow(m_bSetExposure && !STEM);
  /*m_sbcNewSpot.EnableWindow(!STEM);
  m_butSetSpot.EnableWindow(!STEM);
  m_statNewSpot.EnableWindow(!STEM);*/
  switch (m_iWhichRecord) {
  case NO_TS_EXTRA_RECORD:
  case TS_OPPOSITE_TRIAL:
    m_strInstruct1 = "";
    m_strInstruct2 = "";
    m_strEntries = "";
    break;
  case TS_FOCUS_SERIES:
    m_strEntries = mWinApp->mParamIO->EntryListToString(1, 2, mNumExtraFocus, NULL, 
      &mExtraFocus[0]);
    m_strInstruct2 = "Each entry is change in defocus from default";
    m_strInstruct1 = "";
    break;
  case TS_FILTER_SERIES:
    m_strEntries = mWinApp->mParamIO->EntryListToString(2, 1, mNumExtraFilter, 
      &mExtraSlits[0], &mExtraLosses[0]);
    m_strInstruct1 = "Each entry is 2 numbers: -1,0 for unfiltered,";
    m_strInstruct2 = "or slit width (0=default), change in loss";
    break;
  case TS_OTHER_CHANNELS:
    m_strEntries = mWinApp->mParamIO->EntryListToString(3, 0, mNumExtraChannels, 
      &mExtraChannels[0], NULL);
    mWinApp->mCamera->CountSimultaneousChannels(mCamParam, simultaneous, 3, numSimul, 
      numAvail);
    if (!numSimul)
      m_strInstruct1.Format("No available detectors are selected in %s parameters",
        (LPCTSTR)modeNames[3]);
    else if (numSimul == 1)
      m_strInstruct1.Format("Detector %d will be acquired in %s", 
        simultaneous[0], (LPCTSTR)modeNames[3]);
    else {
      str = mWinApp->mParamIO->EntryListToString(3, 0, numSimul, &simultaneous[0], NULL);
      m_strInstruct1 = CString("Detectors to be acquired simultaneously: ") + str;
    }
    m_editEntries.EnableWindow(numAvail > B3DMAX(1, numSimul));
    m_strInstruct2 = "";
    if (numAvail > B3DMAX(1, numSimul))
      m_strInstruct2 = "Enter detectors to acquire in separate shots";
    break;
  }
  UpdateData(false);
}

// Convert the string in the edit box and complain if there are problems
int CTSExtraFile::ValidateRecordEntries(int whichRecord)
{
  CString message;
  int numSimul, numAvail, simultaneous[3];
  int err = 0;
  switch (whichRecord) {
  case NO_TS_EXTRA_RECORD:
  case TS_OPPOSITE_TRIAL:
    break;
  case TS_FOCUS_SERIES:
    err = mWinApp->mParamIO->StringToEntryList(1, m_strEntries, mNumExtraFocus, 
      NULL, &mExtraFocus[0], MAX_EXTRA_RECORDS);
    break;
  case TS_FILTER_SERIES:
    err = mWinApp->mParamIO->StringToEntryList(2, m_strEntries, mNumExtraFilter, 
      mExtraSlits, mExtraLosses, MAX_EXTRA_RECORDS);
    break;
  case TS_OTHER_CHANNELS:
    mWinApp->mCamera->CountSimultaneousChannels(mCamParam, simultaneous, 3, numSimul, 
      numAvail);
    mNumExtraChannels = 0;
    if (numAvail > B3DMAX(1, numSimul)) {
      err = mWinApp->mParamIO->StringToEntryList(3, m_strEntries, mNumExtraChannels, 
        mExtraChannels, NULL, MAX_STEM_CHANNELS);
      if (!err) {
        for (int i = 0; i < mNumExtraChannels; i++) {
          if (mExtraChannels[i] < 1 || mExtraChannels[i] > mCamParam->numChannels) {
            message.Format("Warning: Detector numbers should be between 1 and %d",
              mCamParam->numChannels);
            AfxMessageBox(message, MB_EXCLAME);
            return -1;
          }
        }
      }
    }
    break;
  }
  if (err > 0) {
    message = err == 1 ? "Each entry must have two comma-separated numbers.\n\n" 
      : "The list of entries is too long.\n\n";
    AfxMessageBox(message + "Correct or delete the list of entries before proceeding.",
      MB_EXCLAME);
  }
  if (err < 0)
    AfxMessageBox("Warning: There are more than the expected number of\n"
      "commas or values in at least one of the entries", MB_EXCLAME);
  return err;
}
