// TaskAreaOptionDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "TaskAreaOptionDlg.h"
#include "ComplexTasks.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif


// CTaskAreaOptionDlg dialog


CTaskAreaOptionDlg::CTaskAreaOptionDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_TASK_AREA_OPTIONS, pParent)
  , m_bUseViewNotSearch(FALSE)
  , m_iRoughLM(0)
  , m_iRefine(0)
  , m_iResetIS(0)
  , m_iWalkup(0)
{

}

CTaskAreaOptionDlg::~CTaskAreaOptionDlg()
{
}

void CTaskAreaOptionDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Check(pDX, IDC_CHECK_USE_VIEW, m_bUseViewNotSearch);
  DDX_Radio(pDX, IDC_RROUGH_LM_SEARCH, m_iRoughLM);
  DDX_Radio(pDX, IDC_RREFINE_TRIAL, m_iRefine);
  DDX_Radio(pDX, IDC_RESETIS_TRIAL, m_iResetIS);
  DDX_Radio(pDX, IDC_RWALKUP_TRIAL, m_iWalkup);
}


BEGIN_MESSAGE_MAP(CTaskAreaOptionDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_CHECK_USE_VIEW, OnCheckUseView)
  ON_BN_CLICKED(IDC_RREFINE_TRIAL, OnRrefineTrial)
  ON_BN_CLICKED(IDC_RESETIS_TRIAL, OnResetisTrial)
END_MESSAGE_MAP()


// CTaskAreaOptionDlg message handlers
BOOL CTaskAreaOptionDlg::OnInitDialog()
{
  m_iRoughLM = mWinApp->mComplexTasks->GetFEUseSearchIfInLM() ? 0 : 1;
  m_iRefine = mWinApp->mComplexTasks->GetFEUseTrialInLD() ? 0 : 1;
  m_iResetIS = mWinApp->mComplexTasks->GetRSRAUseTrialInLDMode() ? 0 : 1;
  m_iWalkup = mWinApp->mComplexTasks->GetWalkUseViewInLD() ? 1 : 0;
  m_bUseViewNotSearch = mWinApp->mComplexTasks->GetTasksUseViewNotSearch();
  ManageVorSlabels();
  UpdateData(false);
  return TRUE;
}

// Close button
void CTaskAreaOptionDlg::OnOK()
{
  UpdateData(true);
  mWinApp->mComplexTasks->SetFEUseSearchIfInLM(m_iRoughLM == 0);
  mWinApp->mComplexTasks->SetFEUseTrialInLD(m_iRefine == 0);
  mWinApp->mComplexTasks->SetRSRAUseTrialInLDMode(m_iResetIS == 0);
  mWinApp->mComplexTasks->SetWalkUseViewInLD(m_iWalkup != 0);
  mWinApp->mComplexTasks->SetTasksUseViewNotSearch(m_bUseViewNotSearch);
  CBaseDlg::OnOK();
}

// Cancel
void CTaskAreaOptionDlg::OnCancel()
{
  CBaseDlg::OnCancel();
}

void CTaskAreaOptionDlg::OnRrefineTrial()
{
  UpdateData(true);
  if (!m_iRefine && !mWinApp->mComplexTasks->GetFEWarnedUseTrial()) {
    mWinApp->mComplexTasks->SetFEWarnedUseTrial(true);
    CString message;
    message.Format("The minimum field of view for reliable Fine Eucentricity is\n%.1f "
      "microns.  The procedure could fail if the field of view of a\nTrial image"
      "is much smaller than this and if the stage is not well-behaved.\n\n"
      "Also, the eucentricity will not be set well if the Trial area\n"
      "is not at about the same height as the Record area.\n\n"
      "Are you sure you want to use the Trial area?",
      mWinApp->mComplexTasks->GetMinFEFineField());
    if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) == IDNO) {
      m_iRefine = 1;
      UpdateData(false);
    }
  }
}

void CTaskAreaOptionDlg::OnResetisTrial()
{
  UpdateData(true);
  if (!m_iResetIS && !mWinApp->mComplexTasks->GetRSRAWarnedUseTrial()) {
    mWinApp->mComplexTasks->SetRSRAWarnedUseTrial(true);
    CString message;
    message.Format("The minimum field of view for reliable Reset and Realign is\n%.1f "
      "microns.  The procedure could fail and lose positioning\nif the field of view of"
      " a Trial image is much smaller than this and if the stage is not well-behaved.\n\n"
      "Are you sure you want to use the Trial area?",
      mWinApp->mComplexTasks->GetMinRSRAField());
    if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) == IDNO) {
      m_iResetIS = 1;
      UpdateData(false);
    }
  }
}

void CTaskAreaOptionDlg::OnCheckUseView()
{
  UpdateData(true);
  ManageVorSlabels();
}

void CTaskAreaOptionDlg::ManageVorSlabels()
{
  CString str = m_bUseViewNotSearch ? "View" : "View or Search";
  SetDlgItemText(IDC_RROUGHLM_V_OR_S, str);
  SetDlgItemText(IDC_RREFINE_V_OR_S, str);
  SetDlgItemText(IDC_RRESETIS_V_OR_S, str);
  SetDlgItemText(IDC_RWALKUP_V_OR_S, str);
  SetDlgItemText(IDC_STAT_ROUGH_VORS, str);
}
