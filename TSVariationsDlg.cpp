// TSVariationsDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "TSController.h"
#include "CameraController.h"
#include ".\TSVariationsDlg.h"


// CTSVariationsDlg dialog

CTSVariationsDlg::CTSVariationsDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CTSVariationsDlg::IDD, pParent)
  , m_bPlusMinus(FALSE)
  , m_fAngle(0)
  , m_fValue(0)
  , m_iType(0)
  , m_iLinear(0)
  , m_fSeriesStep(0)
  , m_strPower(_T(""))
  , m_bNumFramesFixed(FALSE)
{
  mInitialized = false;
}

CTSVariationsDlg::~CTSVariationsDlg()
{
}

void CTSVariationsDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDOK, m_butOK);
  DDX_Control(pDX, IDCANCEL, m_butCancel);
  DDX_Control(pDX, IDC_LIST_TSV_TABLE, m_listViewer);
  DDX_Control(pDX, IDC_EDIT_TSV_ANGLE, m_editAngle);
  DDX_Control(pDX, IDC_CHECK_TSV_PLUS_MINUS, m_butPlusMinus);
  DDX_Control(pDX, IDC_EDIT_TSV_VALUE, m_editValue);
  DDX_Radio(pDX, IDC_TSV_EXPOSURE, m_iType);
  DDX_Radio(pDX, IDC_TSV_FIXED_VALUE, m_iLinear);
  DDX_Check(pDX, IDC_CHECK_TSV_PLUS_MINUS, m_bPlusMinus);
  DDX_Text(pDX, IDC_EDIT_TSV_ANGLE, m_fAngle);
  DDV_MinMaxFloat(pDX, m_fAngle, -100.f, 100.f);
  DDX_Text(pDX, IDC_EDIT_TSV_VALUE, m_fValue);
  DDX_Control(pDX, IDC_TSV_EXPOSURE, m_butExposure);
  DDX_Control(pDX, IDC_TSV_DRIFT, m_butDrift);
  DDX_Control(pDX, IDC_TSV_FRAME_TIME, m_butFrameTime);
  DDX_Control(pDX, IDC_TSV_DEFOCUS, m_butFocus);
  DDX_Control(pDX, IDC_TSV_ENERGY, m_butLoss);
  DDX_Control(pDX, IDC_TSV_SLIT_WIDTH, m_butSlit);
  DDX_Control(pDX, IDC_TSV_FIXED_VALUE, m_butFixed);
  DDX_Control(pDX, IDC_TSV_LINEAR_CHANGE, m_butLinear);
  DDX_Control(pDX, IDC_BUT_TSV_ADD_ITEM, m_butAddItem);
  DDX_Control(pDX, IDC_BUT_TSV_REMOVE, m_butRemove);
  DDX_Text(pDX, IDC_EDIT_TSV_SERIES_STEP, m_fSeriesStep);
  DDV_MinMaxFloat(pDX, m_fSeriesStep, 1.05f, 2.0f);
  DDX_Control(pDX, IDC_SPIN_TSV_SERIES_POWER, m_sbcSeriesPower);
  DDX_Text(pDX, IDC_STAT_SERIES_POWER, m_strPower);
  DDX_Control(pDX, IDC_BUT_TSV_ADD_SERIES, m_butAddSeries);
  DDX_Control(pDX, IDC_EDIT_TSV_SERIES_STEP, m_editSeriesStep);
  DDX_Control(pDX, IDC_CHECK_TSV_NFRAMES_FIXED, m_butNumFamesFixed);
  DDX_Check(pDX, IDC_CHECK_TSV_NFRAMES_FIXED, m_bNumFramesFixed);
}


BEGIN_MESSAGE_MAP(CTSVariationsDlg, CBaseDlg)
  ON_WM_SIZE()
  ON_EN_KILLFOCUS(IDC_EDIT_TSV_ANGLE, OnKillfocusEditTsvAngle)
  ON_EN_KILLFOCUS(IDC_EDIT_TSV_VALUE, OnKillfocusEditTsvValue)
  ON_BN_CLICKED(IDC_CHECK_TSV_PLUS_MINUS, OnCheckTsvPlusMinus)
  ON_BN_CLICKED(IDC_TSV_EXPOSURE, OnTsvExposure)
  ON_BN_CLICKED(IDC_TSV_DRIFT, OnTsvExposure)
  ON_BN_CLICKED(IDC_TSV_FRAME_TIME, OnTsvExposure)
  ON_BN_CLICKED(IDC_TSV_ENERGY, OnTsvExposure)
  ON_BN_CLICKED(IDC_TSV_SLIT_WIDTH, OnTsvExposure)
  ON_BN_CLICKED(IDC_TSV_DEFOCUS, OnTsvExposure)
  ON_BN_CLICKED(IDC_TSV_FIXED_VALUE, OnTsvFixedValue)
  ON_BN_CLICKED(IDC_TSV_LINEAR_CHANGE, OnTsvFixedValue)
  ON_BN_CLICKED(IDC_BUT_TSV_ADD_ITEM, OnButTsvAddItem)
  ON_BN_CLICKED(IDC_BUT_TSV_REMOVE, OnButTsvRemove)
  ON_LBN_SELCHANGE(IDC_LIST_TSV_TABLE, OnSelchangeListTsvTable)
  ON_BN_CLICKED(IDC_BUT_TSV_ADD_SERIES, OnButTsvAddSeries)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_TSV_SERIES_POWER, OnDeltaposSpinTsvSeriesPower)
  ON_BN_CLICKED(IDC_CHECK_TSV_NFRAMES_FIXED, &CTSVariationsDlg::OnNumFramesFixed)
END_MESSAGE_MAP()


// CTSVariationsDlg message handlers

BOOL CTSVariationsDlg::OnInitDialog()
{
  int fields[6] = {16,24,24,36,24,8};
  int tabs[6], i;
  CBaseDlg::OnInitDialog();
 
  // Get initial size of client area and list box to determine borders for resizing
  CRect wndRect, listRect, OKRect, clientRect;
  GetClientRect(clientRect);
  m_listViewer.GetWindowRect(listRect);
  m_iBorderX = clientRect.Width() - listRect.Width();
  m_iBorderY = clientRect.Height() - listRect.Height();

  GetWindowRect(wndRect);
  int iXoffset = (wndRect.Width() - clientRect.Width()) / 2;
  m_butOK.GetWindowRect(OKRect);
  m_iOKoffset = (OKRect.top - wndRect.top) - (wndRect.Height() - clientRect.Height())
    + iXoffset - listRect.Height();
  m_iOKLeft = OKRect.left - wndRect.left - iXoffset;
  m_butCancel.GetWindowRect(OKRect);
  m_iCancelLeft = OKRect.left - wndRect.left - iXoffset;
  m_butHelp.GetWindowRect(OKRect);
  m_iHelpLeft = OKRect.left - wndRect.left - iXoffset;
  m_iHelpOffset = (OKRect.top - wndRect.top) - (wndRect.Height() - clientRect.Height())
    + iXoffset - listRect.Height();

  tabs[0] = fields[0];
  for (i = 1; i < 6; i++)
    tabs[i] = tabs[i - 1] + fields[i]; 
  m_listViewer.SetTabStops(6, tabs);

  mInitialized = true;
  mSeriesPower = mParam->cosinePower;
  m_strPower.Format("Use cosine to the 1/%d", mSeriesPower);
  m_sbcSeriesPower.SetRange(0, 100);
  m_sbcSeriesPower.SetPos(50);

  mNumVaries = mParam->numVaryItems;
  for (i = 0; i < mNumVaries; i++)
    mVaries[i] = mParam->varyArray[i];
  mCurrentItem = -1;
  FillListBox();   // Calls Update()

  UpdateData(false);
  return TRUE;
}

void CTSVariationsDlg::OnOK() 
{
  UnloadToCurrentItem();
  PurgeVariations(mVaries, mNumVaries, mCurrentItem);
	CBaseDlg::OnOK();
  mParam->numVaryItems = mNumVaries;
  for (int i = 0; i < mNumVaries; i++)
    mParam->varyArray[i] = mVaries[i];
}

void CTSVariationsDlg::OnCancel() 
{
  CBaseDlg::OnCancel();
}

void CTSVariationsDlg::OnSize(UINT nType, int cx, int cy) 
{
  CDialog::OnSize(nType, cx, cy);
  
  if (!mInitialized)
    return;

  int newX = cx - m_iBorderX;
  int newY = cy - m_iBorderY;
  if (newX < 1)
    newX = 1;
  if (newY < 1)
    newY = 1;
  m_listViewer.SetWindowPos(NULL, 0, 0, newX, newY, SWP_NOZORDER | SWP_NOMOVE);

  newY += m_iOKoffset;
  m_butCancel.SetWindowPos(NULL, m_iCancelLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butOK.SetWindowPos(NULL, m_iOKLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butHelp.SetWindowPos(NULL, m_iHelpLeft, newY - m_iOKoffset + m_iHelpOffset, 0, 0, 
    SWP_NOZORDER | SWP_NOSIZE);
}

// Angle or plus/minus changes: resort and fill table
void CTSVariationsDlg::OnKillfocusEditTsvAngle()
{
  UpdateData(true);
  mVaries[mCurrentItem].angle = m_fAngle;
  FillListBox();
}

void CTSVariationsDlg::OnCheckTsvPlusMinus()
{
  UpdateData(true);
  mVaries[mCurrentItem].plusMinus = m_bPlusMinus;
  FillListBox();
}

void CTSVariationsDlg::OnNumFramesFixed()
{
  UpdateData(true);
}

// Other changes: just update the string
void CTSVariationsDlg::OnKillfocusEditTsvValue()
{
  UpdateData(true);
  CheckItemValue(true);
  mVaries[mCurrentItem].value = m_fValue;
  UpdateListString(mCurrentItem);
}

void CTSVariationsDlg::OnTsvExposure()
{
  UpdateData(true);
  CheckItemValue(true);
  mVaries[mCurrentItem].type = m_iType;
  mVaries[mCurrentItem].value = m_fValue;
  UpdateListString(mCurrentItem);
}

void CTSVariationsDlg::OnTsvFixedValue()
{
  UpdateData(true);
  mVaries[mCurrentItem].linear = m_iLinear != 0;
  UpdateListString(mCurrentItem);
}

// Add an item with minimal values
void CTSVariationsDlg::OnButTsvAddItem()
{
  UnloadToCurrentItem();
  PurgeVariations(mVaries, mNumVaries, mCurrentItem);
  if (mCurrentItem >= 0) {
    mVaries[mNumVaries].angle = mVaries[mCurrentItem].angle;
    mVaries[mNumVaries].value = mVaries[mCurrentItem].value;
    mVaries[mNumVaries].linear = mVaries[mCurrentItem].linear;
    mVaries[mNumVaries].plusMinus = mVaries[mCurrentItem].plusMinus;
    mVaries[mNumVaries++].type = mVaries[mCurrentItem].type;
  } else {
    AddOneVariation(0, 0., 1.);
  }
  mCurrentItem = mNumVaries - 1;
  FillListBox();
  Update();
}

// Adds one bidirectional fixed value
void CTSVariationsDlg::AddOneVariation(int type, float angle, float value)
{
  mVaries[mNumVaries].angle = angle;
  mVaries[mNumVaries].value = value;
  mVaries[mNumVaries].linear = false;
  mVaries[mNumVaries].plusMinus = true;
  mVaries[mNumVaries++].type = type;
}

// Remove current item
void CTSVariationsDlg::OnButTsvRemove()
{
  UnloadToCurrentItem();
  for (int i = mCurrentItem + 1; i < mNumVaries; i++)
    mVaries[i-1] = mVaries[i];
  mNumVaries--;
  m_listViewer.DeleteString(mCurrentItem);
  if (mCurrentItem >= mNumVaries)
    mCurrentItem--;
  m_listViewer.SetCurSel(mCurrentItem);
  if (PurgeVariations(mVaries, mNumVaries, mCurrentItem))
    FillListBox();
  Update();
}

void CTSVariationsDlg::OnButTsvAddSeries()
{
  int i, j;
  double angle, fps, factor = 1.;
  int *active = mWinApp->GetActiveCameraList();
  ControlSet *conSet = mWinApp->GetCamConSets() + active[mParam->cameraIndex] * 
    MAX_CONSETS + RECORD_CONSET;
  CameraParameters *camParam = mWinApp->GetCamParams() + active[mParam->cameraIndex];
  float newExp, baseTime = 0.001f, recExp = conSet->exposure;
  float frameTime = conSet->frameTime;
  bool fixedFrames = camParam->K2Type && conSet->doseFrac && m_bNumFramesFixed;
  bool oneFrame = false;
  UpdateData(true);
  
  // Constrain the exposure time
  mWinApp->mCamera->ConstrainExposureTime(camParam, conSet->doseFrac > 0, 
    conSet->K2ReadMode, conSet->binning, conSet->alignFrames && !conSet->useFrameAlign, 
    mWinApp->mCamera->DESumCountForConstraints(camParam, conSet), recExp, frameTime);
  if (camParam->K2Type) {
    if (conSet->doseFrac && fabs(recExp - frameTime) < 1.e-5) {
      baseTime = mWinApp->mCamera->GetK2ReadoutInterval(camParam->K2Type);
      oneFrame = true;
      fixedFrames = false;
    } else if (fixedFrames) {
      baseTime = (recExp / frameTime) * 
        mWinApp->mCamera->GetK2ReadoutInterval(camParam->K2Type);
    } else {
      baseTime = mWinApp->mCamera->GetLastK2BaseTime();
    }
    CString mess;
    mess.Format("With your current settings for the K2/K3 camera,\n"
      " exposure times are constrained to a multiple of %.4f sec\n\n"
      "If this is not OK, change the parameter settings or the selected camera",
      baseTime);
    if (AfxMessageBox(mess, MB_OKCANCEL | MB_ICONINFORMATION) == IDCANCEL)
      return;
  } else if (IS_FALCON2_OR_3(camParam)) {
    baseTime = mWinApp->mCamera->GetFalconReadoutInterval();
  } else if (mWinApp->mDEToolDlg.HasFrameTime(camParam)) {
    fps = camParam->DE_FramesPerSec > 0 ? camParam->DE_FramesPerSec :
      mWinApp->mDEToolDlg.GetFramesPerSecond();
    if (conSet->K2ReadMode > 0 && camParam->DE_CountingFPS > 0)
      fps = camParam->DE_CountingFPS;
    if (fps > 0.)
      baseTime = (float)(1. / fps);
  }

  // Remove any existing exposure entries, and frame entries if doing dose fraac at all
  for (i = 0, j = 0; i < mNumVaries; i++)
    if (mVaries[i].type != TS_VARY_EXPOSURE && 
      (!conSet->doseFrac || mVaries[i].type != TS_VARY_FRAME_TIME))
      mVaries[j++] = mVaries[i];
  mNumVaries = j;
  if (mNumVaries > MAX_TS_VARIES - 2) {
    AfxMessageBox("Cannot add a series because there are too many\n"
      "scheduled changes for the program's array", MB_EXCLAME);
    FillListBox();
    return;
  }

  // Add one item at zero tilt
  AddOneVariation(TS_VARY_EXPOSURE, 0., recExp);

  // Increase the basic factor and find angle where that occurs
  while (true) {
    factor *= m_fSeriesStep;
    newExp = (float)(B3DNINT(factor * recExp / baseTime) * baseTime);
    while (newExp <= mVaries[mNumVaries - 1].value)
      newExp += baseTime;
    factor = newExp / recExp;
    angle = acos(1. / pow(factor, (double)mSeriesPower)) / DTOR;
    if (angle > mTopAngle + 2.)
      break;
    if (mNumVaries + (fixedFrames ? 1 : 0) >= MAX_TS_VARIES) {
      AfxMessageBox("The series is incomplete because there are too many\n"
        "scheduled changes for the program's array", MB_EXCLAME);
      break;
    }
    angle = B3DNINT(10. * angle) / 10.;
    AddOneVariation(TS_VARY_EXPOSURE, (float)angle, newExp);
    if (fixedFrames)
      AddOneVariation(TS_VARY_FRAME_TIME, (float)angle, (float)(factor * frameTime));
  }
  mCurrentItem = mNumVaries - 1;
  FillListBox();
  Update();
}

void CTSVariationsDlg::OnDeltaposSpinTsvSeriesPower(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int newVal = mSeriesPower + pNMUpDown->iDelta;
  if (newVal < 1 || newVal > MAX_COSINE_POWER) {
    *pResult = 1;
    return;
  }
	mSeriesPower = newVal;
  m_strPower.Format("Use cosine to the 1/%d", mSeriesPower);
  UpdateData(false);
  *pResult = 0;
}

// Change in item selected in table
void CTSVariationsDlg::OnSelchangeListTsvTable()
{
  int newCur = m_listViewer.GetCurSel();
  UnloadToCurrentItem();
  mCurrentItem = newCur;
  if (mCurrentItem == LB_ERR)
    mCurrentItem = -1;
  if (mCurrentItem >= 0)
    m_listViewer.SetCurSel(mCurrentItem);
  if (PurgeVariations(mVaries, mNumVaries, mCurrentItem))
    FillListBox();
  Update();
}

// Enable dialog if there is current item; load its properties
void CTSVariationsDlg::Update(void)
{
  int *active = mWinApp->GetActiveCameraList();
  ControlSet *conSet = mWinApp->GetCamConSets() + active[mParam->cameraIndex] * 
    MAX_CONSETS + RECORD_CONSET;

  m_butPlusMinus.EnableWindow(mCurrentItem >= 0);
  m_editAngle.EnableWindow(mCurrentItem >= 0);
  m_editValue.EnableWindow(mCurrentItem >= 0);
  m_butExposure.EnableWindow(mCurrentItem >= 0 && !mDoingTS);
  m_butFrameTime.EnableWindow(mCurrentItem >= 0 && !mDoingTS && mK2Selected && 
    conSet->doseFrac);
  m_butNumFamesFixed.EnableWindow(mCurrentItem >= 0 && !mDoingTS && mK2Selected && 
    conSet->doseFrac);
  m_butFixed.EnableWindow(mCurrentItem >= 0);
  m_butLinear.EnableWindow(mCurrentItem >= 0);
  m_butDrift.EnableWindow(mCurrentItem >= 0 && !mSTEMmode && !mDoingTS);
  m_butFocus.EnableWindow(mCurrentItem >= 0 && !mSTEMmode && !mDoingTS);
  m_butLoss.EnableWindow(mCurrentItem >= 0 && mFilterExists && !mDoingTS);
  m_butSlit.EnableWindow(mCurrentItem >= 0 && mFilterExists && !mDoingTS);
  m_butRemove.EnableWindow(mCurrentItem >= 0 && !mDoingTS);
  m_butAddItem.EnableWindow(mNumVaries < MAX_TS_VARIES && !mDoingTS);
  m_butAddSeries.EnableWindow(!mDoingTS);
  m_sbcSeriesPower.EnableWindow(!mDoingTS);
  m_editSeriesStep.EnableWindow(!mDoingTS);
  if (mCurrentItem < 0)
    return;
  m_fAngle = mVaries[mCurrentItem].angle;
  m_fValue = mVaries[mCurrentItem].value;
  m_bPlusMinus = mVaries[mCurrentItem].plusMinus;
  m_iLinear = mVaries[mCurrentItem].linear ? 1 : 0;
  m_iType = mVaries[mCurrentItem].type;
  UpdateData(false);
}

// Update string for the given item
void CTSVariationsDlg::UpdateListString(int index)
{
  CString string;
  ItemToListString(index, string);
  m_listViewer.DeleteString(index);
  m_listViewer.InsertString(index, string);
  if (mCurrentItem >= 0)
    m_listViewer.SetCurSel(mCurrentItem);
}

// Format the string for the table
void CTSVariationsDlg::ItemToListString(int index, CString &string)
{
  char *typeNames[] = {"Exposure", "Drift", "Defocus", "Energy", "Slit", "Frame"};
  int numDec[] = {3, 2, 2, 1, 0, 4};
  int type = mVaries[index].type;
  CString format, valstr;
  format.Format("%%.%df", numDec[type]);
  valstr.Format(format, mVaries[index].value);
  string.Format("%s\t%.1f\t%s\t%s\t", mVaries[index].plusMinus ? "+/-" : " ", 
    mVaries[index].angle, mVaries[index].linear ? "linear" : "fixed", typeNames[type]);
  string += valstr;
  if ((!mFilterExists && (type == TS_VARY_SLIT || type == TS_VARY_ENERGY)) || 
    (mSTEMmode && (type == TS_VARY_FOCUS || type == TS_VARY_DRIFT)))
    string += "\tNA";
}

// Resort the data and refill the list box
void CTSVariationsDlg::FillListBox(void)
{
  CString string;
  int i;
  m_listViewer.ResetContent();
  if (mNumVaries) {

    // Resort the data by angle
    SortVariations(mVaries, mNumVaries, mCurrentItem);

    for (i = 0; i < mNumVaries; i++) {
      ItemToListString(i, string);
      m_listViewer.AddString(string);
    }
    if (mCurrentItem < 0)
      mCurrentItem = 0;
    m_listViewer.SetCurSel(mCurrentItem);
  } else
    mCurrentItem = -1;

  Update();
}

// Move data from controls into the current item
void CTSVariationsDlg::UnloadToCurrentItem(void)
{
  bool changed;
  if (mCurrentItem < 0)
    return;
  UpdateData(true);
  changed = CheckItemValue(false);
  mVaries[mCurrentItem].angle = m_fAngle;
  mVaries[mCurrentItem].value = m_fValue;
  mVaries[mCurrentItem].plusMinus = m_bPlusMinus;
  mVaries[mCurrentItem].linear = m_iLinear != 0;
  mVaries[mCurrentItem].type = m_iType;
  UpdateListString(mCurrentItem);
}

// Check the current value and limit it if necessary
bool CTSVariationsDlg::CheckItemValue(bool update)
{
  bool changed = false;
  int *active = mWinApp->GetActiveCameraList();
  CameraParameters *camParam = mWinApp->GetCamParams() + active[mParam->cameraIndex];
  FilterParams *filtp = mWinApp->GetFilterParams();
  switch (m_iType) {
    case TS_VARY_DRIFT:
      if (m_fValue < 0.) {
        m_fValue = 0.;
        changed = true;
      }
      break;

    case TS_VARY_EXPOSURE:
      if (m_fValue < 0.001) {
        m_fValue = 0.001f;
        changed = true;
      }
      break;

    case TS_VARY_FRAME_TIME:
      changed = mWinApp->mCamera->ConstrainFrameTime(m_fValue, camParam->K2Type);
      break;

    case TS_VARY_SLIT:
      if (m_fValue < filtp->minWidth) {
        m_fValue = filtp->minWidth;
        changed = true;
      }
      if (m_fValue > filtp->maxWidth) {
        m_fValue = filtp->maxWidth;
        changed = true;
      }
      break;
        
    case TS_VARY_ENERGY:
      if (m_fValue < filtp->minLoss) {
        m_fValue = filtp->minLoss;
        changed = true;
      }
      if (m_fValue > filtp->maxLoss) {
        m_fValue = filtp->maxLoss;
        changed = true;
      }
      break;
  }
  if (changed && update)
    UpdateData(false);
  return changed;
}

///////////////////////////////////
//  STATIC FUNCTIONS

// Remove or resolve duplicate entries in scheduled changes, maintain a current item
// if any.  Return true if changes were made.  Also sorts them and retains sort
bool CTSVariationsDlg::PurgeVariations(VaryInSeries *varies, int &numVaries, 
                                    int &currentItem)
{
  int i, j, k;
  bool changed = false;
  double angle1, angle2;
  SortVariations(varies, numVaries, currentItem);
  for (i = 0; i < numVaries - 1; ) {
    bool retained = true;
    for (j = i + 1; j < numVaries; j++) {
      angle1 = varies[i].angle;
      angle2 = varies[j].angle;
      if (fabs(fabs(angle1) - fabs(angle2)) < 0.9 && varies[i].type == varies[j].type) {
        if (varies[j].plusMinus || (!varies[i].plusMinus && !varies[j].plusMinus && 
          (angle1 < 0. && angle2 < 0. || angle1 >= 0. && angle2 >= 0.))) {

          // Delete the first item if the later one is +/- or if neither is and their
          // angles have the same sign
          for (k = i + 1; k < numVaries; k++)
            varies[k-1] = varies[k];
          numVaries--;
          if (currentItem > i)
            currentItem--;
          retained = false;
          changed = true;
          break;
        } else if (varies[i].plusMinus) {
          
          // Otherwise if the earlier one is +/- (later one is not) modify earlier one
          varies[i].plusMinus = false;
          if (angle1 < 0 && angle2 < 0 || angle1 >= 0. && angle2 >= 0.)
            varies[i].angle = -varies[i].angle;
          changed = true;
        }
      }
    }
    if (retained)
      i++;
  }
  if (changed)
    SortVariations(varies, numVaries, currentItem);
  return changed;
}

// Resort the variations by angle, maintaining a current item if any
void CTSVariationsDlg::SortVariations(VaryInSeries *varies, int &numVaries, 
                                   int &currentItem)
{
  float angle1, angle2;
  VaryInSeries temp;
  int i, j;
  for (i = 0; i < numVaries - 1; i++) {
    for (j = i + 1; j < numVaries; j++) {
      angle1 = varies[i].angle;
      if (varies[i].plusMinus && angle1 > 0)
        angle1 = -angle1;
      angle2 = varies[j].angle;
      if (varies[j].plusMinus && angle2 > 0)
        angle2 = -angle2;
      if (angle2 < angle1) {
        temp = varies[i];
        varies[i] = varies[j];
        varies[j] = temp;
        if (currentItem == i)
          currentItem = j;
        else if (currentItem == j)
          currentItem = i;
      }
    }
  }
}

// Fill in the array indicating which types vary, and find any zero-degree values
// if zeroDegValues is not NULL.  Eliminates types inappropriate for the scope mode
// Returns false if there is nothing to vary
bool CTSVariationsDlg::ListVaryingTypes(VaryInSeries *varyArray, int numVaries,
                                        int *typeVaries, float *zeroDegValues)
{
  int i, type, numv = 0;
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  BOOL STEM = winApp->GetSTEMMode();
  BOOL EFTEM = winApp->GetEFTEMMode();
  for (i = 0; i < MAX_VARY_TYPES; i++)
    typeVaries[i] = 0;
  if (!numVaries)
    return false;
  for (i = 0; i < numVaries; i++) {
    type = varyArray[i].type;
    if ((!EFTEM && (type == TS_VARY_SLIT || type == TS_VARY_ENERGY)) || 
      (STEM && (type == TS_VARY_FOCUS || type == TS_VARY_DRIFT)))
      continue;
    typeVaries[type] = 1;
    numv++;
    if (zeroDegValues && fabs((double)varyArray[i].angle) < 0.9)
      zeroDegValues[type] = varyArray[i].value;
  }
  return numv > 0;
}

// Compute the values that should be set at the current angle
// This time typeVaries is set to -1 for linearly varying items
bool CTSVariationsDlg::FindValuesToSet(VaryInSeries *varyArray, int numVaries,
                                       float *zeroDegValues, float angle, 
                                       int *typeVaries, float *outValues)
{
  int j, indBelow, indAbove, sign, neg, itype;
  double dist, distBelow, distAbove, angBelow, angAbove, vangle;
  float valBelow;
  if (!ListVaryingTypes(varyArray, numVaries, typeVaries, NULL))
    return false;

  // Loop on the varying types
  for (itype = 0; itype < MAX_VARY_TYPES; itype++) {
    if (!typeVaries[itype])
      continue;

    // Find the nearest one below and above if any
    distBelow = distAbove = 1.e10;
    indBelow = indAbove = -1;
    for (j = 0; j < numVaries; j++) {
      if (varyArray[j].type == itype) {
        neg = 1;
        if (varyArray[j].plusMinus)
          neg = -1;
        for (sign = neg; sign <= 1; sign += 2) {
          vangle = sign * varyArray[j].angle;
          if (vangle * angle >= 0) {
            dist = fabs((double)angle) - fabs(vangle);
            if (dist >= 0 && dist < distBelow) {
              distBelow = dist;
              indBelow = j;
              angBelow = vangle;
            } else if (dist < 0 && -dist < distAbove) {
              distAbove = -dist;
              indAbove = j;
              angAbove = vangle;
            }
          }
        }
      }
    }

    // Interpolate if there is one above and either there is one below that is linear,
    // or there is no one below and the one above is linear
    if (indAbove >= 0 && ((indBelow >= 0 && varyArray[indBelow].linear) ||
      (indBelow < 0 && varyArray[indAbove].linear))) {
      if (indBelow < 0) {
        valBelow = zeroDegValues[itype];
        angBelow = 0.;
      } else
        valBelow = varyArray[indBelow].value;

      if (fabs(angAbove - angBelow) < 0.1)
        outValues[itype] = valBelow;
      else
        outValues[itype] = (float)(valBelow + (angle - angBelow) * 
        (varyArray[indAbove].value - valBelow) / (angAbove - angBelow));
      typeVaries[itype] = -1;
    } else {
        
      // Otherwise, use the fixed value below
      if (indBelow < 0)
        outValues[itype] = zeroDegValues[itype];
      else
        outValues[itype] = varyArray[indBelow].value;
      typeVaries[itype] = 1;
    }
  }
  return true;
}
