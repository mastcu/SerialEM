// CNavRotAlignDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\NavRotAlignDlg.h"
#include "NavigatorDlg.h"
#include "Navhelper.h"
#include "EMbufferManager.h"
#include "ShiftManager.h"
#include "CameraController.h"


// CNavRotAlignDlg dialog

CNavRotAlignDlg::CNavRotAlignDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CNavRotAlignDlg::IDD, pParent)
  , m_fCenterAngle(0)
  , m_bSearchAngles(FALSE)
  , m_fSearchRange(0)
{
  mNonModal = true;
}

CNavRotAlignDlg::~CNavRotAlignDlg()
{
}

void CNavRotAlignDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_EDIT_SEARCH_RANGE, m_editSearchRange);
  DDX_Text(pDX, IDC_EDIT_SEARCH_RANGE, m_fSearchRange);
  DDV_MinMaxFloat(pDX, m_fSearchRange, 0.f, 50.f);
  DDX_Text(pDX, IDC_EDIT_CENTER_ANGLE, m_fCenterAngle);
  DDV_MinMaxFloat(pDX, m_fCenterAngle, -180.f, 180.f);
  DDX_Check(pDX, IDC_CHECK_SEARCH_RANGE, m_bSearchAngles);
  DDX_Control(pDX, IDC_BUT_TRANSFORM, m_butTransform);
  DDX_Control(pDX, IDC_BUT_APPLY_IS, m_butApplyIS);
  DDX_Control(pDX, IDC_BUT_ALIGN_TO_MAP, m_butAlign);
}


BEGIN_MESSAGE_MAP(CNavRotAlignDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_BUT_ALIGN_TO_MAP, OnButAlignToMap)
  ON_BN_CLICKED(IDC_BUT_APPLY_IS, OnButApplyIs)
  ON_BN_CLICKED(IDC_BUT_TRANSFORM, OnOK)
END_MESSAGE_MAP()


// CNavRotAlignDlg message handlers

BOOL CNavRotAlignDlg::OnInitDialog()
{
  mImBufs = mWinApp->GetImBufs();
  mBufManager = mWinApp->mBufferManager;
  mHelper = mWinApp->mNavHelper;
  mNav = mWinApp->mNavigator;
  CBaseDlg::OnInitDialog();
  int refInd = mBufManager->GetBufToReadInto();

  // Get the map ID and the stage position of the reference item
  mRefMapID = mImBufs[refInd].mMapID;
  CMapDrawItem *item = mWinApp->mNavigator->FindItemWithMapID(mRefMapID);
  mRefStageX = item->mStageX;
  mRefStageY = item->mStageY;

  PrepareToUseImage();
  mCurrentReg = mNav->GetCurrentRegistration();
  mStartingReg = mImBufs[refInd].mRegistration;
  m_fCenterAngle = mHelper->GetRotAlignCenter();
  m_fSearchRange = mHelper->GetRotAlignRange();
  m_bSearchAngles = mHelper->GetSearchRotAlign();

  UpdateData(false);
  m_butTransform.EnableWindow(false);
  m_butApplyIS.EnableWindow(false);
  SetDefID(45678);    // Disable Transform from being default button   
  return FALSE;
}

void CNavRotAlignDlg::PostNcDestroy() 
{
  delete this;  
  CDialog::PostNcDestroy();
}

void CNavRotAlignDlg::OnOK() 
{
  float shiftX, shiftY, backX, backY, delX, delY, dxy[2];
  int numDone;
  CString mess;
  ScaleMat rMat, aMat, aInv;
  UnloadData();
  if (!mWinApp->mNavigator || mImBufs->mTimeStamp != mRotTimeStamp || 
    mImBufs[1].mTimeStamp != mOrigTimeStamp || 
    mCurrentReg != mNav->GetCurrentRegistration() || 
    !mNav->BufferStageToImage(&mImBufs[1], aMat, delX, delY)) {
    AfxMessageBox(mWinApp->mNavigator ? "You have changed the current registration or "
      "the image in the A or B buffer.\nThe transformation cannot be applied" : 
      "You cannot use this dialog after closing the Navigator", MB_EXCLAME);
  } else {

    // Back-rotate the position in aligned image that is aligned to center of reference
    // The position in right-handed coords is -shiftX, +shiftY (double inversion of Y)
    // And the image coordinates is position + X, -Y
    mImBufs->mImage->getShifts(shiftX, shiftY);
    rMat = mNav->GetRotationMatrix(-mLastRotation, false);
    backX = (float)mImBufs->mImage->getWidth() / 2.f + 
		(-rMat.xpx * shiftX + rMat.xpy * shiftY);
    backY = (float)mImBufs->mImage->getHeight() / 2.f - 
      (-rMat.ypx * shiftX + rMat.ypy * shiftY);

    // Adjust if necessary and convert to a stage coordinate, 
    mNav->AdjustMontImagePos(&mImBufs[1], backX, backY);
    aInv = MatInv(aMat);
    shiftX = aInv.xpx * (backX - delX) + aInv.xpy * (backY - delY);
    shiftY = aInv.ypx * (backX - delX) + aInv.ypy * (backY - delY);

    // Modify the rotation matrix by an underlying stretch in stage coordinates
    rMat = mWinApp->mShiftManager->StretchCorrectedRotation(-mLastRotation);
    dxy[0] = shiftX - rMat.xpx * mRefStageX - rMat.xpy * mRefStageY;
    dxy[1] = shiftY - rMat.ypx * mRefStageX - rMat.ypy * mRefStageY;
    numDone = mNav->TransformFromRotAlign(mStartingReg, rMat, dxy);
    mess.Format("WARNING: This transformation is of limited accuracy over long distances"
      "\r\n  %d items transformed with rotation and shift", numDone);
    mWinApp->AppendToLog(mess);
  }
  mHelper->GetRotAlignPlacement();
  mHelper->mRotAlignDlg = NULL;
  DestroyWindow();
}

void CNavRotAlignDlg::OnButApplyIs()
{
  float shiftX, shiftY, backX, backY;
  int camera = mImBufs[1].mCamera ? mImBufs[1].mCamera : mWinApp->GetCurrentCamera();
  ScaleMat rMat;
  if (mImBufs->mTimeStamp != mRotTimeStamp || mImBufs[1].mTimeStamp != mOrigTimeStamp ||
    !mImBufs[1].mMagInd || !mImBufs[1].mBinning) {
      AfxMessageBox(((!mImBufs[1].mMagInd || !mImBufs[1].mBinning) ? 
        "There is not enough information to determine the shift on the scope" : 
      "You have changed the the image in the A or B buffer.") +
      CString("\n\nThe image shift cannot be applied"), MB_EXCLAME);
  } else {

    // Back-rotate the shift in aligned image to unrotated image and impose it
    // Here we back-rotate the position, which is -shiftX, shiftY in right-handed coords,
    // then take the negative to get to a right-handed shift
    mImBufs->mImage->getShifts(shiftX, shiftY);
    rMat = mNav->GetRotationMatrix(-mLastRotation, false);
    backX = (float)mImBufs[1].mBinning * (rMat.xpx * shiftX - rMat.xpy * shiftY);
    backY = (float)mImBufs[1].mBinning * (rMat.ypx * shiftX - rMat.ypy * shiftY);
    mWinApp->mShiftManager->ImposeImageShiftOnScope(backX, backY, mImBufs[1].mMagInd, 
      camera, false, false);
  }
  m_butApplyIS.EnableWindow(false);
  mAppliedIS = true;
  mWinApp->RestoreViewFocus();
}

void CNavRotAlignDlg::OnCancel() 
{
  UnloadData();
  mHelper->GetRotAlignPlacement();
  mHelper->mRotAlignDlg = NULL;
  DestroyWindow();
}

void CNavRotAlignDlg::OnButAlignToMap()
{
  float shiftX, shiftY, range = 0.;
  ScaleMat aMat;
  CString mess;
  int registration, topBuf;
  UpdateData(true);
  SetDefID(45678);
  mWinApp->RestoreViewFocus();
  if (!mWinApp->mNavigator || 
    mImBufs[mBufManager->GetBufToReadInto()].mMapID != mRefMapID) {
    AfxMessageBox(mWinApp->mNavigator ? "The image being aligned to has changed.\n"
      "You need to reopen this dialog to proceed" : 
      "You cannot use this dialog after closing the Navigator", MB_EXCLAME);
    OnCancel();
    return;
  }

  // If there is a new image in B, check the registration but proceed to use it.
  if (mImBufs[1].mTimeStamp != mOrigTimeStamp) {
    topBuf = mHelper->BufferForRotAlign(registration);
    if (registration != mCurrentReg || registration != mNav->GetCurrentRegistration()) {
      mess.Format("You have changed the current registration or the registration of the "
        "image to be aligned.\n\nYou need an image at registration %d to proceed", 
        mCurrentReg);
      AfxMessageBox(mess, MB_EXCLAME);
      return;
    }
    if (mImBufs[topBuf].mCaptured == BUFFER_PROCESSED || 
      mImBufs[topBuf].mCaptured == BUFFER_FFT) {
      AfxMessageBox("You cannot align a processed image; you need a new image to proceed"
        , MB_EXCLAME);
      return;
    }
    PrepareToUseImage();
  }

  if (m_bSearchAngles)
    range = m_fSearchRange;
  mBufManager->CopyImageBuffer(1, 0);
  if (mHelper->AlignWithRotation(mBufManager->GetBufToReadInto(),
    m_fCenterAngle, range, mLastRotation, shiftX, shiftY)) {
    AfxMessageBox("The alignment procedure failed with the current parameters",
      MB_EXCLAME);
    m_butTransform.EnableWindow(false);
    return;
  }
  aMat = mWinApp->mShiftManager->StretchCorrectedRotation(mImBufs->mCamera, mImBufs->mMagInd, 
    mLastRotation);
  mHelper->TransformBuffer(mImBufs, aMat, mImBufs->mImage->getWidth(), 
    mImBufs->mImage->getHeight(), 0., aMat);
  mImBufs->mImage->setShifts(shiftX, shiftY);
  mImBufs->mTimeStamp = 0.001 * GetTickCount();
  mImBufs->mCaptured = BUFFER_PROCESSED;
  mRotTimeStamp = mImBufs->mTimeStamp;
  mWinApp->SetCurrentBuffer(0);
  m_butTransform.EnableWindow(true);
  m_butApplyIS.EnableWindow(!mAppliedIS);
}

void CNavRotAlignDlg::UnloadData(void)
{
  UpdateData(true);
  mHelper->SetRotAlignCenter(m_fCenterAngle);
  mHelper->SetRotAlignRange(m_fSearchRange);
  mHelper->SetSearchRotAlign(m_bSearchAngles);
}

void CNavRotAlignDlg::PrepareToUseImage(void)
{
  int registration;
  float shiftX, shiftY;
  int topBuf = mHelper->BufferForRotAlign(registration);

  // Clear out an image shift in the image to simplify applying image shift later
  mImBufs[topBuf].mImage->getShifts(shiftX, shiftY);
  if (shiftX || shiftY)
    mWinApp->mShiftManager->SetAlignShifts(0., 0., false, &mImBufs[topBuf],
      !mWinApp->mScope->GetNoScope());

  // Copy into B with rolling if it is not an overview in B, and save time stamp
  if (!topBuf) {
    for (int i = mBufManager->GetShiftsOnAcquire(); i > 0 ; i--)
      mBufManager->CopyImageBuffer(i - 1, i);
  }
  mOrigTimeStamp = mImBufs[1].mTimeStamp;
  mAppliedIS = false;
}

// Disable action buttons if doing tasks: this is called by helper
void CNavRotAlignDlg::Update(void)
{
  BOOL enable = !mWinApp->DoingTasks() && !mWinApp->mCamera->CameraBusy();
  m_butTransform.EnableWindow(enable);
  m_butApplyIS.EnableWindow(enable);
  m_butAlign.EnableWindow(enable);
}
