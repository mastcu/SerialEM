// ReadFileDlg.cpp : a non-modal dialog to read repeatedly from current file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include ".\ReadFileDlg.h"
#include "CameraController.h"
#include "EMmontageController.h"
#include "EMbufferManager.h"


// CReadFileDlg dialog

CReadFileDlg::CReadFileDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CReadFileDlg::IDD, pParent)
  , m_iSection(0)
{
  mNonModal = true;
}

CReadFileDlg::~CReadFileDlg()
{
}

void CReadFileDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDOK, m_butOK);
  DDX_Control(pDX, IDC_BUTREADSEC, m_butRead);
  DDX_Control(pDX, IDC_BUTNEXTSEC, m_butNext);
  DDX_Control(pDX, IDC_BUTPREVSEC, m_butPrevious);
  DDX_Text(pDX, IDC_EDITSECTION, m_iSection);
  DDV_MinMaxInt(pDX, m_iSection, 0, 1000000);
  DDX_Control(pDX, IDC_EDITSECTION, m_editSection);
}


BEGIN_MESSAGE_MAP(CReadFileDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_BUTREADSEC, OnButReadsec)
  ON_BN_CLICKED(IDC_BUTNEXTSEC, OnButNextSec)
  ON_BN_CLICKED(IDC_BUTPREVSEC, OnButPrevSec)
END_MESSAGE_MAP()


// CReadFileDlg message handlers

BOOL CReadFileDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  UpdateData(false);
  m_editSection.SetFocus();
  m_editSection.SetSel(0, -1);
   
  return FALSE;
}

void CReadFileDlg::PostNcDestroy() 
{
  delete this;  
  CDialog::PostNcDestroy();
}

// When closing, get docwnd to record placement and then set pointer to NULL
void CReadFileDlg::OnOK() 
{
  ReadSection(0);
  mWinApp->mDocWnd->GetReadDlgPlacement();
  mWinApp->mDocWnd->mReadFileDlg = NULL;
  DestroyWindow();
}

void CReadFileDlg::OnCancel() 
{
  mWinApp->mDocWnd->GetReadDlgPlacement();
  mWinApp->mDocWnd->mReadFileDlg = NULL;
  DestroyWindow();
}

// Buttons just call the read routine
void CReadFileDlg::OnButReadsec()
{
  ReadSection(0);
}

void CReadFileDlg::OnButNextSec()
{ 
  ReadSection(1);
}

void CReadFileDlg::OnButPrevSec()
{
  ReadSection(-1);
}

// Disable buttons when busy
void CReadFileDlg::Update(void)
{
  BOOL enable = !mWinApp->DoingTasks() && !mWinApp->mCamera->CameraBusy() && 
    mWinApp->mStoreMRC != NULL;
  m_butRead.EnableWindow(enable);
  m_butNext.EnableWindow(enable);
  m_butPrevious.EnableWindow(enable);
  m_butOK.EnableWindow(enable);
}

// Read a section if legal
void CReadFileDlg::ReadSection(int delta)
{ 
  int newsec, limit;
  bool readOK = false;
  MontParam *param = mWinApp->GetMontParam();
  if (mWinApp->DoingTasks() || mWinApp->mCamera->CameraBusy() || !mWinApp->mStoreMRC ||
    mWinApp->mBufferManager->CheckAsyncSaving())
    return;
  UpdateData(true);
  newsec = m_iSection + delta;
  limit = mWinApp->Montaging() ? param->zMax : (mWinApp->mStoreMRC->getDepth() - 1);
  if (newsec < 0 || newsec > limit) {
    AfxMessageBox("Section number out of range for current file", MB_EXCLAME);
    newsec = B3DMAX(0, B3DMIN(limit, newsec));
  } else
    readOK = true;

  if (newsec != m_iSection) {
    m_iSection = newsec;
    UpdateData(false);
  }

  // Setting the focus back to the edit works better than setting pushbutton styles for
  // retaining OK as the default button
  m_editSection.SetFocus();
  mWinApp->RestoreViewFocus();
  if (readOK) {
    if (mWinApp->Montaging()) {
      mWinApp->mMontageController->ReadMontage(newsec, param, mWinApp->mStoreMRC);
    } else {

      // Reproduce mdocwnd->readfromfile
      mWinApp->mBufferManager->ReadFromFile(mWinApp->mStoreMRC, newsec);
      mWinApp->DrawReadInImage();
    }
  }
}
