// EMbufferwindow.cpp:    Has buttons for copying and saving buffers and controls
//                           to govern the behavior of buffers
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "EMbufferManager.h"
#include ".\EMbufferWindow.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include "ShiftCalibrator.h"
#include "CameraController.h"
#include "BaseDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static BOOL protectBuffer[MAX_BUFFERS];

/////////////////////////////////////////////////////////////////////////////
// CEMbufferWindow dialog


CEMbufferWindow::CEMbufferWindow(CWnd* pParent /*=NULL*/)
  : CToolDlg(CEMbufferWindow::IDD, pParent)
  , m_strMemory(_T(""))
{
  SEMBuildTime(__DATE__, __TIME__);
  //{{AFX_DATA_INIT(CEMbufferWindow)
  m_bAlignOnSave = FALSE;
  m_bConfirmDestroy = FALSE;
  m_bCopyOnSave = FALSE;
	m_bAlignToB = FALSE;
	m_strToFile = _T("");
	//}}AFX_DATA_INIT
  for (int i = 0; i < MAX_BUFFERS; i++)
    protectBuffer[i] = false;
}


void CEMbufferWindow::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CEMbufferWindow)
  DDX_Control(pDX, IDC_STATTOFILE, m_statToFile);
  DDX_Control(pDX, IDC_SPINTOFILE, m_sbcToFile);
  DDX_Control(pDX, IDC_ALIGNTOB, m_butAlignToB);
  DDX_Control(pDX, IDC_CONFIRMDESTROY, m_butConfirmDestroy);
  DDX_Control(pDX, IDC_BSAVEACTIVE, m_butSaveActive);
  DDX_Control(pDX, IDC_BSAVEA, m_butSaveA);
  DDX_Control(pDX, IDC_BCOPYD, m_butCopyD);
  DDX_Control(pDX, IDC_BCOPYC, m_butCopyC);
  DDX_Control(pDX, IDC_BCOPYB, m_butCopyB);
  DDX_Control(pDX, IDC_BCOPYANY, m_butCopyAny);
  DDX_Control(pDX, IDC_BCOPYA, m_butCopyA);
  DDX_Control(pDX, IDC_SPINREAD, m_sbcRead);
  DDX_Control(pDX, IDC_SPINROLL, m_sbcRoll);
  DDX_Control(pDX, IDC_SPINCOPY, m_sbcCopy);
  DDX_Check(pDX, IDC_CONFIRMDESTROY, m_bConfirmDestroy);
  DDX_Check(pDX, IDC_ALIGNTOB, m_bAlignToB);
  DDX_Text(pDX, IDC_STATTOFILE, m_strToFile);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_DELETEBUF, m_butDelete);
  DDX_Text(pDX, IDC_STATMEMORY, m_strMemory);
}


BEGIN_MESSAGE_MAP(CEMbufferWindow, CToolDlg)
  //{{AFX_MSG_MAP(CEMbufferWindow)
  ON_BN_CLICKED(IDC_BCOPYA, OnBcopya)
  ON_BN_CLICKED(IDC_BCOPYB, OnBcopyb)
  ON_BN_CLICKED(IDC_BCOPYC, OnBcopyc)
  ON_BN_CLICKED(IDC_BCOPYD, OnBcopyd)
  ON_BN_CLICKED(IDC_BCOPYANY, OnBcopyany)
  ON_BN_CLICKED(IDC_BSAVEA, OnBsavea)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINCOPY, OnDeltaposSpincopy)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINROLL, OnDeltaposSpinroll)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINREAD, OnDeltaposSpinread)
  ON_BN_CLICKED(IDC_CONFIRMDESTROY, OnConfirmdestroy)
  ON_BN_CLICKED(IDC_BSAVEACTIVE, OnBsaveactive)
	ON_BN_CLICKED(IDC_ALIGNTOB, OnAligntob)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPINTOFILE, OnDeltaposSpintofile)
	//}}AFX_MSG_MAP
  ON_BN_CLICKED(IDC_DELETEBUF, OnClickedDeletebuf)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEMbufferWindow message handlers

void CEMbufferWindow::DoCopyBuffer(int iWhich)
{
  EMimageBuffer *fromBuf = mWinApp->mActiveView->GetActiveImBuf();
  if (!fromBuf)
    return;
  EMimageBuffer *toBuf = mImBufs + iWhich;
  mBufferManager->CopyImBuf(fromBuf, toBuf);
  mWinApp->RestoreViewFocus();
}

void CEMbufferWindow::OnBcopya() 
{
  DoCopyBuffer(0);
}

void CEMbufferWindow::OnBcopyb() 
{
  DoCopyBuffer(1);
}

void CEMbufferWindow::OnBcopyc() 
{
  DoCopyBuffer(2);  
}

void CEMbufferWindow::OnBcopyd() 
{
  DoCopyBuffer(3);  
}

void CEMbufferWindow::OnBcopyany() 
{
  DoCopyBuffer(mBufferManager->GetFifthCopyToBuf());
}

/* This sends a direct message for new window
void CEMbufferWindow::OnBcopynew() 
{
  mWinApp->RestoreViewFocus();
} */

void CEMbufferWindow::OnBsavea() 
{
  mWinApp->mDocWnd->SaveRegularBuffer();  
  mWinApp->RestoreViewFocus();
}

void CEMbufferWindow::OnBsaveactive() 
{
  mWinApp->mDocWnd->SaveActiveBuffer(); 
  mWinApp->RestoreViewFocus();
}

// Set text based on spin button value
void CEMbufferWindow::SetSpinText(UINT inID, int newVal)
{
  char letter = 'A' + newVal;
  CString string = letter;
  SetDlgItemText(inID, string);
}

// The spin button for variable copy button
void CEMbufferWindow::OnDeltaposSpincopy(NMHDR* pNMHDR, LRESULT* pResult) 
{
  mWinApp->RestoreViewFocus();
  int newVal;
  if (NewSpinnerValue(pNMHDR, pResult, 4, MAX_BUFFERS - 1, newVal))
    return;
  mBufferManager->SetFifthCopyToBuf(newVal);
  SetSpinText(IDC_BCOPYANY, newVal);

  // Manage copy button state here
  EMimageBuffer *activeBuf = mWinApp->mActiveView->GetActiveImBuf();
  BOOL bEnable = ((!mWinApp->DoingTasks() ||
    (mWinApp->mShiftCalibrator->CalibratingOffset() && mWinApp->mCamera->CameraReady()))
    && activeBuf != NULL && activeBuf->mImage != NULL && !protectBuffer[newVal]);
  m_butCopyAny.EnableWindow(bEnable && mImBufs + newVal != activeBuf );
  
  *pResult = 0;
}

// Spin button for the buffers to roll
void CEMbufferWindow::OnDeltaposSpinroll(NMHDR* pNMHDR, LRESULT* pResult) 
{
  mWinApp->RestoreViewFocus();
  int newVal;
  int bufDiff = mWinApp->LowDoseMode() ? 3 : 2;

  // Limit the value to MAX_ROLL_BUFFERS -1 to allow for read and copy on save and more
  if (NewSpinnerValue(pNMHDR, pResult, 0, MAX_ROLL_BUFFERS - 1, newVal))
    return;
  SetSpinText(IDC_ROLLBUF, newVal);
  int oldRoll = mBufferManager->GetShiftsOnAcquire();
  int oldRead = mBufferManager->GetBufToReadInto();
  mBufferManager->SetShiftsOnAcquire(newVal); 

  // If the buffer to read into is still 2 past the roll buffer, maintain
  // that relationship to the new roll buffer
  if (oldRoll + bufDiff == oldRead) {
    mBufferManager->SetBufToReadInto(newVal + bufDiff);
    SetSpinText(IDC_READBUF, newVal + bufDiff);
    m_sbcRead.SetPos(newVal + bufDiff);
  }
  if (m_bCopyOnSave)
    mBufferManager->SetCopyOnSave(newVal + 1);
  ManageCopyOnSave(newVal + 1);
  mWinApp->mAlignFocusWindow.UpdateSettings();

  *pResult = 0;
}

// Manage the copy on Save message
void CEMbufferWindow::ManageCopyOnSave(int inVal)
{
  char letter = 'A' + inVal;
  CString string = "Align to B instead of ";
  if (mWinApp->LowDoseMode()) {
    string = string + letter + "/";
    letter++;
  }
  string += letter;
  SetDlgItemText(IDC_ALIGNTOB, string);
}

// Spin button for buffer to read into
void CEMbufferWindow::OnDeltaposSpinread(NMHDR* pNMHDR, LRESULT* pResult) 
{
  mWinApp->RestoreViewFocus();
  int newVal;
  if (NewSpinnerValue(pNMHDR, pResult, 0, MAX_BUFFERS - 1, newVal))
    return;
  SetSpinText(IDC_READBUF, newVal);
  mBufferManager->SetBufToReadInto(newVal); 
  *pResult = 0;
}

// The spin button for file to write to
void CEMbufferWindow::OnDeltaposSpintofile(NMHDR* pNMHDR, LRESULT* pResult) 
{
  mWinApp->RestoreViewFocus();
  int newVal;
  if (NewSpinnerValue(pNMHDR, pResult, mWinApp->mDocWnd->GetCurrentStore(),
    0, mWinApp->mDocWnd->GetNumStores() - 1, newVal))
    return;
  mWinApp->mDocWnd->SetCurrentStore(newVal);
	*pResult = 0;
}

// Confirm Record before destroy checkbox
void CEMbufferWindow::OnConfirmdestroy() 
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
  mBufferManager->SetConfirmBeforeDestroy(3, m_bConfirmDestroy ? 1 : 0);
}

// When align to B is checked, modify align-focus window
void CEMbufferWindow::OnAligntob() 
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
 	mBufferManager->SetAlignToB(m_bAlignToB);
  mWinApp->mAlignFocusWindow.UpdateSettings();
}

BOOL CEMbufferWindow::OnInitDialog() 
{
  CToolDlg::OnInitDialog();

  // Get pointers
  mBufferManager = mWinApp->mBufferManager;
  mImBufs = mWinApp->GetImBufs();

  // Fix up the confirm destroy string
  CString *modeNames = mWinApp->GetModeNames();
  CString confirmText = "Protect unsaved ";
  confirmText += modeNames[3] + " images";
  SetDlgItemText(IDC_CONFIRMDESTROY, confirmText);

  // Set ranges of spin buttons
  m_sbcCopy.SetRange(4, MAX_BUFFERS);
  m_sbcRead.SetRange(0, MAX_BUFFERS);
  m_sbcRoll.SetRange(0, MAX_BUFFERS);
  m_sbcToFile.SetRange(-1, MAX_STORES);
  m_sbcCopy.SetPos(mBufferManager->GetFifthCopyToBuf());
  SetSpinText(IDC_BCOPYANY, mBufferManager->GetFifthCopyToBuf());

  // Do everything else the same if external settings change
  UpdateSettings();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

// Set up the various buttons that might be affected by a change of settings
void CEMbufferWindow::UpdateSettings()
{
  // Load variables 
  m_bAlignOnSave = (mBufferManager->GetAlignOnSave() != 0);
  m_bConfirmDestroy = (mBufferManager->GetConfirmBeforeDestroy(3) != 0);
  m_bCopyOnSave = (mBufferManager->GetCopyOnSave() != 0);
  m_bAlignToB = mBufferManager->GetAlignToB();

  int nRoll = mBufferManager->GetShiftsOnAcquire();
  // Set up spin buttons
  m_sbcRoll.SetPos(nRoll);
  m_sbcRead.SetPos(mBufferManager->GetBufToReadInto());

  // Manage text of companions to spin buttons
  SetSpinText(IDC_ROLLBUF, nRoll);
  SetSpinText(IDC_READBUF, mBufferManager->GetBufToReadInto());

  // Set up copy on save text and set buffer if checked
  ManageCopyOnSave(nRoll + 1);
  if (m_bCopyOnSave)
    mBufferManager->SetCopyOnSave(nRoll + 1);

  // Rely on this for updateData
  UpdateSaveCopy();
}

// Check the status of the buffer and enable/disable copy and save buttons
void CEMbufferWindow::UpdateSaveCopy()
{
// If doing tasks, only the copy to new button should be active
  BOOL bEnable, noTasks;
  int maxStore, curStore, mbint;
  int spinBuf = m_sbcCopy.GetPos() & 255;
  EMimageBuffer *activeBuf = mWinApp->mActiveView->GetActiveImBuf();

  noTasks = !mWinApp->DoingTasks() || 
    (mWinApp->mShiftCalibrator->CalibratingOffset() && mWinApp->mCamera->CameraReady());
  bEnable = noTasks && activeBuf != NULL && activeBuf->mImage != NULL;
  m_butCopyA.EnableWindow(bEnable && mImBufs != activeBuf && !protectBuffer[0]);
  m_butCopyB.EnableWindow(bEnable && mImBufs + 1 != activeBuf && !protectBuffer[1]);
  m_butCopyC.EnableWindow(bEnable && mImBufs + 2 != activeBuf && !protectBuffer[2]);
  m_butCopyD.EnableWindow(bEnable && mImBufs + 3 != activeBuf && !protectBuffer[3]);
  m_butCopyAny.EnableWindow(bEnable && mImBufs + spinBuf != activeBuf
     && !protectBuffer[spinBuf]);
  m_butDelete.EnableWindow(bEnable && ActiveBufNumIfDeletable() >= 0);
  
  //m_butCopyNew.EnableWindow(bEnable);
  CButton *bNew = (CButton *)GetDlgItem(ID_WINDOW_NEW);
  bNew->EnableWindow(activeBuf != NULL && activeBuf->mImage != NULL);

  m_butSaveA.EnableWindow(noTasks && !mWinApp->StartedTiltSeries() &&
    mBufferManager->IsBufferSavable(mBufferManager->GetSaveBuffer()));
  m_butSaveActive.EnableWindow(noTasks && !mWinApp->StartedTiltSeries()
    && mBufferManager->IsBufferSavable(mWinApp->mActiveView->GetActiveImBuf()));

  // If doing complex tasks, disable the roll spin bar, read buffer and the
  // protect record, copy on save and align on save options
  bEnable = !mWinApp->DoingComplexTasks();
  m_sbcRoll.EnableWindow(bEnable);
  m_sbcRead.EnableWindow(bEnable);
  m_butAlignToB.EnableWindow(bEnable);
  m_butConfirmDestroy.EnableWindow(bEnable);

  // Update the file spin button
  maxStore = mWinApp->mDocWnd->GetNumStores() - 1;
  if (maxStore < 0) 
    maxStore = 0;
  curStore = mWinApp->mDocWnd->GetCurrentStore();
  if (curStore < 0)
    curStore = 0;
  m_strToFile.Format("To file %d", curStore + 1);
  m_sbcToFile.EnableWindow(maxStore > 0 && noTasks);
  m_sbcToFile.SetPos(curStore);

  // Update the memory usage
  mbint = (int)(MemorySummary() / 1.e6);
  m_strMemory.Format("Memory = %d MB", mbint);

  UpdateData(false);
}

void CEMbufferWindow::ProtectBuffer(int inBuf, BOOL inProtect)
{
  if (inBuf >= 0 && inBuf < MAX_BUFFERS) {
    protectBuffer[inBuf] = inProtect;
    UpdateSaveCopy();
  }
}

// Delete the image in a buffer if all conditions satisfied
void CEMbufferWindow::OnClickedDeletebuf()
{
  int bufNum = ActiveBufNumIfDeletable();
  mWinApp->RestoreViewFocus();
  if (bufNum < 0)
    return;
  if (!mBufferManager->OKtoDestroy(bufNum, "Deleting"))
    return;

  // Delete the pixmap image too
  if (mImBufs[bufNum].mPixMap)
    mImBufs[bufNum].mPixMap->doneWithRect();
  if (mImBufs[bufNum].mFiltPixMap)
    mImBufs[bufNum].mFiltPixMap->doneWithRect();
  mImBufs[bufNum].DeleteImage();
  mImBufs[bufNum].DeleteOffsets();
  mImBufs[bufNum].SetImageChanged(1);
  mWinApp->mMainView->SetCurrentBuffer(bufNum);
  mWinApp->UpdateBufferWindows();
}

// Return index of active buffer if it is OK to delete it; otherwise -1
int CEMbufferWindow::ActiveBufNumIfDeletable(void)
{
  EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  if (!imBuf)
    return -1;
  int bufNum = mBufferManager->MainImBufIndex(imBuf);
  if (bufNum < 0)
    return -1;
  if (protectBuffer[bufNum] || !imBuf->mImage)
    return -1;
  return bufNum;
}

double CEMbufferWindow::MemorySummary(void)
{
  double sum = 0.;
  CPtrArray refArray;
  POSITION pos = mWinApp->mDocWnd->GetFirstViewPosition();
  refArray.SetSize(0, 10);
  while (pos != NULL) {
    CSerialEMView *pView = (CSerialEMView *)mWinApp->mDocWnd->GetNextView(pos);
    sum += pView->BufferMemoryUsage(&refArray);
  }
  sum += mWinApp->mCamera->RefMemoryUsage();
  return sum;
}
