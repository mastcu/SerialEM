// NavAcquireDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "EMscope.h"
#include "Mailer.h"
#include ".\NavAcquireDlg.h"
#include "NavigatorDlg.h"


// CNavAcquireDlg dialog

CNavAcquireDlg::CNavAcquireDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CNavAcquireDlg::IDD, pParent)
  , m_iAcquireType(0)
  , m_bRestoreState(FALSE)
  , m_bRoughEucen(FALSE)
  , m_bRealign(FALSE)
  , m_bFineEucen(FALSE)
  , m_bAutofocus(FALSE)
  , m_bCloseValves(FALSE)
  , m_bCookSpec(FALSE)
  , m_bCenterBeam(FALSE)
  , m_bSendEmail(FALSE)
  , m_bPremacro(FALSE)
  , m_bGroupFocus(FALSE)
  , m_strSavingFate(_T(""))
  , m_strFileSavingInto(_T(""))
{
}

CNavAcquireDlg::~CNavAcquireDlg()
{
}

void CNavAcquireDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Radio(pDX, IDC_NA_ACQUIREMAP, m_iAcquireType);
  DDX_Control(pDX, IDC_NA_RESTORE_STATE, m_butRestoreState);
  DDX_Check(pDX, IDC_NA_RESTORE_STATE, m_bRestoreState);
  DDX_Check(pDX, IDC_NA_ROUGH_EUCEN, m_bRoughEucen);
  DDX_Check(pDX, IDC_NA_FINE_EUCEN, m_bFineEucen);
  DDX_Check(pDX, IDC_NA_AUTOFOCUS, m_bAutofocus);
  DDX_Check(pDX, IDC_NA_REALIGNITEM, m_bRealign);
  DDX_Control(pDX, IDC_NA_CLOSE_VALVES, m_butCloseValves);
  DDX_Check(pDX, IDC_NA_CLOSE_VALVES, m_bCloseValves);
  DDX_Check(pDX, IDC_NA_COOKSPECIMEN, m_bCookSpec);
  DDX_Check(pDX, IDC_NA_CENTERBEAM, m_bCenterBeam);
  DDX_Control(pDX, IDC_NA_SENDEMAIL, m_butSendEmail);
  DDX_Check(pDX, IDC_NA_SENDEMAIL, m_bSendEmail);
  DDX_Check(pDX, IDC_NA_PREMACRO, m_bPremacro);
  DDX_Control(pDX, IDC_NA_ACQUIRE_TS, m_butAcquireTS);
  DDX_Control(pDX, IDC_NA_RUNMACRO, m_butRunMacro);
  DDX_Control(pDX, IDC_NA_ACQUIREMAP, m_butAcquireMap);
  DDX_Control(pDX, IDC_NA_JUSTACQUIRE, m_butJustAcquire);
  DDX_Control(pDX, IDC_NA_PREMACRO, m_butPremacro);
  DDX_Control(pDX, IDC_NA_GROUP_FOCUS, m_butGroupFocus);
  DDX_Check(pDX, IDC_NA_GROUP_FOCUS, m_bGroupFocus);
  DDX_Control(pDX, IDC_COMBO_PREMACRO, m_comboPremacro);
  DDX_Control(pDX, IDC_COMBO_MACRO, m_comboMacro);
  DDX_Text(pDX, IDC_STAT_SAVING_FATE, m_strSavingFate);
  DDX_Text(pDX, IDC_STAT_FILE_SAVING_INTO, m_strFileSavingInto);
}


BEGIN_MESSAGE_MAP(CNavAcquireDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_NA_REALIGNITEM, OnNaRealignitem)
  ON_BN_CLICKED(IDC_NA_ACQUIREMAP, OnNaAcquiremap)
  ON_BN_CLICKED(IDC_NA_JUSTACQUIRE, OnNaAcquiremap)
  ON_BN_CLICKED(IDC_NA_RUNMACRO, OnNaAcquiremap)
  ON_BN_CLICKED(IDC_NA_ACQUIRE_TS, OnNaAcquiremap)
  ON_BN_CLICKED(IDC_NA_PREMACRO, OnPremacro)
  ON_BN_CLICKED(IDC_NA_AUTOFOCUS, OnNaAutofocus)
  ON_CBN_SELENDOK(IDC_COMBO_PREMACRO, OnSelendokComboPremacro)
  ON_CBN_SELENDOK(IDC_COMBO_MACRO, OnSelendokComboMacro)
END_MESSAGE_MAP()


// CNavAcquireDlg message handlers
void CNavAcquireDlg::OnOK() 
{
  UpdateData(true);
  mParam->macroIndex = mMacroNum;
  mParam->acqAutofocus = m_bAutofocus;
  mParam->acqFocusOnceInGroup = m_bGroupFocus;
  mParam->acqRoughEucen = m_bRoughEucen;
  mParam->acqRealign = m_bRealign;
  mParam->acqFineEucen = m_bFineEucen;
  mParam->acqCenterBeam = m_bCenterBeam;
  mParam->acqCookSpec = m_bCookSpec;
  mParam->acqRestoreOnRealign = m_bRestoreState;
  mParam->acqCloseValves = m_bCloseValves;
  mParam->acqSendEmail = m_bSendEmail;
  UnloadTSdependentFromDlg(m_iAcquireType);
	CBaseDlg::OnOK();
}

void CNavAcquireDlg::UnloadTSdependentFromDlg(int acquireType)
{
  if (acquireType == ACQUIRE_DO_TS) {
    mParam->preMacroInd = mPremacNum;
    mParam->acqRunPremacro = m_bPremacro;
    mParam->acqSendEmail = m_bSendEmail;
  } else {
    mParam->preMacroIndNonTS = mPremacNum;
    mParam->acqRunPremacroNonTS = m_bPremacro;
    mParam->acqSendEmailNonTS = m_bSendEmail;
  }
}

BOOL CNavAcquireDlg::OnInitDialog() 
{
  int ind;
  CString str;
	CBaseDlg::OnInitDialog();
  mParam = mWinApp->GetNavParams();
  m_butRestoreState.EnableWindow(m_bRealign && m_iAcquireType == ACQUIRE_RUN_MACRO);
  m_butAcquireTS.EnableWindow(m_iAcquireType == ACQUIRE_DO_TS);
  m_butAcquireMap.EnableWindow(mAnyAcquirePoints);
  m_butJustAcquire.EnableWindow(mAnyAcquirePoints);
  m_butRunMacro.EnableWindow(mAnyAcquirePoints);
  m_butGroupFocus.EnableWindow(m_bAutofocus);
  mMacroNum = mParam->macroIndex;
  m_bAutofocus = mParam->acqAutofocus;
  m_bGroupFocus = mParam->acqFocusOnceInGroup;
  m_bRoughEucen = mParam->acqRoughEucen;
  m_bRealign = mParam->acqRealign;
  m_bFineEucen = mParam->acqFineEucen;
  m_bCenterBeam = mParam->acqCenterBeam;
  m_bCookSpec = mParam->acqCookSpec;
  m_bRestoreState = mParam->acqRestoreOnRealign;
  m_bCloseValves = mParam->acqCloseValves;
  CString *names = mWinApp->GetMacroNames();
  for (ind = 0; ind < MAX_MACROS; ind++) {
    if (names[ind].IsEmpty())
      str.Format("# %d", ind + 1);
    else
      str = names[ind];
    m_comboPremacro.AddString((LPCTSTR)str);
    m_comboMacro.AddString((LPCTSTR)str);
  }
  SetDropDownHeight(&m_comboPremacro, MAX_MACROS);
  SetDropDownHeight(&m_comboMacro, MAX_MACROS);
  LoadTSdependentToDlg();
  ManageOutputFile();

  if (mWinApp->mScope->GetNoColumnValve())
    ReplaceWindowText(&m_butCloseValves, "Close column valves", "Turn off filament");
  else if (JEOLscope)
    ReplaceWindowText(&m_butCloseValves, "column valves", "gun valve");
  else if (HitachiScope)
    m_butCloseValves.EnableWindow(false);
  m_butSendEmail.EnableWindow(mWinApp->mMailer->GetInitialized() && 
    !(mWinApp->mMailer->GetSendTo()).IsEmpty());
  return TRUE;
}

void CNavAcquireDlg::LoadTSdependentToDlg(void)
{
  if (m_iAcquireType == ACQUIRE_DO_TS) {
    mPremacNum = mParam->preMacroInd;
    m_bPremacro = mParam->acqRunPremacro;
    m_bSendEmail = mParam->acqSendEmail;
  } else {
    mPremacNum = mParam->preMacroIndNonTS;
    m_bPremacro = mParam->acqRunPremacroNonTS;
    m_bSendEmail = mParam->acqSendEmailNonTS;
  }
  ManageMacro();
}

void CNavAcquireDlg::OnNaRealignitem()
{
  UpdateData(true);
  m_butRestoreState.EnableWindow(m_bRealign && m_iAcquireType == ACQUIRE_RUN_MACRO);
  UpdateData(false);
}

void CNavAcquireDlg::OnNaAcquiremap()
{
  int oldAcquire = m_iAcquireType;
  UpdateData(true);
  UnloadTSdependentFromDlg(oldAcquire);
  LoadTSdependentToDlg();
  m_butRestoreState.EnableWindow(m_bRealign && m_iAcquireType == ACQUIRE_RUN_MACRO);
  ManageMacro();
  ManageOutputFile();
}

void CNavAcquireDlg::OnPremacro()
{
  UpdateData(true);
  ManageMacro();
}

void CNavAcquireDlg::ManageMacro(void)
{
  m_comboPremacro.SetCurSel(mPremacNum - 1);
  m_comboMacro.SetCurSel(mMacroNum - 1);
  m_comboMacro.EnableWindow(m_iAcquireType == ACQUIRE_RUN_MACRO);
  m_comboPremacro.EnableWindow(m_bPremacro);
  UpdateData(false);
}

void CNavAcquireDlg::OnNaAutofocus()
{
  UpdateData(true);
  m_butGroupFocus.EnableWindow(m_bAutofocus);
}


void CNavAcquireDlg::OnSelendokComboPremacro()
{
  mPremacNum = m_comboPremacro.GetCurSel() + 1;
}


void CNavAcquireDlg::OnSelendokComboMacro()
{
  mMacroNum = m_comboMacro.GetCurSel() + 1;
}

void CNavAcquireDlg::ManageOutputFile(void)
{
  CString dir;
  if (m_iAcquireType == ACQUIRE_TAKE_MAP || m_iAcquireType == ACQUIRE_IMAGE_ONLY) {
    if (mNumAcqBeforeFile && mOutputFile.IsEmpty()) {
        m_strSavingFate = "No output file is open for first acquire item!";
        m_strFileSavingInto = "Cancel and define an output file";
    } else {
      m_strSavingFate.Format("%s will%s be saved into the file:", 
        mIfMontOutput ? "Montages" : "Images", 
        ((mNumAcqBeforeFile && mNumFilesToOpen) || mNumFilesToOpen > 1) ? 
        " initially" : "");
      UtilSplitPath(mOutputFile, dir, m_strFileSavingInto);
    }
  } else {
    m_strFileSavingInto = "";
    m_strSavingFate = "";
  }
  UpdateData(false);
}
