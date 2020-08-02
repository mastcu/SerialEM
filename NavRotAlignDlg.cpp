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

// Class statics (yes they have to be defined here)
double CNavRotAlignDlg::mOrigTimeStamp;
float CNavRotAlignDlg::mRefStageX;
float CNavRotAlignDlg::mRefStageY;
EMimageBuffer *CNavRotAlignDlg::mImBufs;
CNavHelper *CNavRotAlignDlg::mHelper;
CSerialEMApp *CNavRotAlignDlg::mWinApp;
EMbufferManager *CNavRotAlignDlg::mBufManager;
bool CNavRotAlignDlg::mAppliedIS;
int CNavRotAlignDlg::mRefMapID;
int CNavRotAlignDlg::mStartingReg;
float CNavRotAlignDlg::mLastRotation;

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

void CNavRotAlignDlg::InitializeStatics(void)
{
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mImBufs = mWinApp->GetImBufs();
  mBufManager = mWinApp->mBufferManager;
  mHelper = mWinApp->mNavHelper;
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
  mNav = mWinApp->mNavigator;
  CBaseDlg::OnInitDialog();
  CheckAndSetupReference();
  PrepareToUseImage();
  mCurrentReg = mNav->GetCurrentRegistration();
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
  float delX, delY;
  int numDone;
  CString mess;
  ScaleMat aMat;
  UnloadData();
  if (!mWinApp->mNavigator || mImBufs->mTimeStamp != mRotTimeStamp || 
    mImBufs[1].mTimeStamp != mOrigTimeStamp || 
    mCurrentReg != mNav->GetCurrentRegistration() || 
    !mNav->BufferStageToImage(&mImBufs[1], aMat, delX, delY)) {
    AfxMessageBox(mWinApp->mNavigator ? "You have changed the current registration or "
      "the image in the A or B buffer.\nThe transformation cannot be applied" : 
      "You cannot use this dialog after closing the Navigator", MB_EXCLAME);
  } else {
    numDone = TransformItemsFromAlignment();

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
  float bestRot, range = 0.;
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
  if (AlignToMap(m_fCenterAngle, range, bestRot)) {
    m_butTransform.EnableWindow(false);
    return;
  }
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

// Set up to use the current image to align to the reference, getting it into B if it
// is not an overview
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

// The first function to call, finds the nav item corresponding to the image in the 
// read buffer (returns -1 if none), gets its stage position and returns its registration
int CNavRotAlignDlg::CheckAndSetupReference(void)
{
  int refInd = mBufManager->GetBufToReadInto();

  // Get the map ID and the stage position of the reference item
  mRefMapID = mImBufs[refInd].mMapID;
  CMapDrawItem *item = mWinApp->mNavigator->FindItemWithMapID(mRefMapID);
  if (!item)
    return -1;
  mRefStageX = item->mStageX;
  mRefStageY = item->mStageY;
  mStartingReg = mImBufs[refInd].mRegistration;
  return mStartingReg;
}

// Align the current image in B to the reference, including transforming it to line up
int CNavRotAlignDlg::AlignToMap(float centerAngle, float range, float &bestRot)
{
  float shiftX, shiftY;
  ScaleMat aMat;
  mBufManager->CopyImageBuffer(1, 0);
  if (mHelper->AlignWithRotation(mBufManager->GetBufToReadInto(),
    centerAngle, range, mLastRotation, shiftX, shiftY)) {
    SEMMessageBox("The alignment procedure failed with the current parameters",
      MB_EXCLAME);
    return 1;
  }
  aMat = mWinApp->mShiftManager->StretchCorrectedRotation(mImBufs->mCamera, 
    mImBufs->mMagInd, mLastRotation);
  mHelper->TransformBuffer(mImBufs, aMat, mImBufs->mImage->getWidth(),
    mImBufs->mImage->getHeight(), 0., aMat);
  mImBufs->mImage->setShifts(shiftX, shiftY);
  mImBufs->mTimeStamp = 0.001 * GetTickCount();
  mImBufs->mCaptured = BUFFER_PROCESSED;
  bestRot = mLastRotation;
  return 0;
}

// Do the transformation of items into the new registration
int CNavRotAlignDlg::TransformItemsFromAlignment(void)
{
  CNavigatorDlg *nav = mWinApp->mNavigator;
  float shiftX, shiftY, backX, backY, delX, delY, dxy[2];
  int numDone;
  ScaleMat rMat, aMat, aInv;

  if (!nav->BufferStageToImage(&mImBufs[1], aMat, delX, delY))
    return -1;

  // Back-rotate the position in aligned image that is aligned to center of reference
  // The position in right-handed coords is -shiftX, +shiftY (double inversion of Y)
  // And the image coordinates is position + X, -Y
  mImBufs->mImage->getShifts(shiftX, shiftY);
  rMat = nav->GetRotationMatrix(-mLastRotation, false);
  backX = (float)mImBufs->mImage->getWidth() / 2.f +
    (-rMat.xpx * shiftX + rMat.xpy * shiftY);
  backY = (float)mImBufs->mImage->getHeight() / 2.f -
    (-rMat.ypx * shiftX + rMat.ypy * shiftY);

  // Adjust if necessary and convert to a stage coordinate, 
  nav->AdjustMontImagePos(&mImBufs[1], backX, backY);
  aInv = MatInv(aMat);
  shiftX = aInv.xpx * (backX - delX) + aInv.xpy * (backY - delY);
  shiftY = aInv.ypx * (backX - delX) + aInv.ypy * (backY - delY);

  // Modify the rotation matrix by an underlying stretch in stage coordinates
  rMat = mWinApp->mShiftManager->StretchCorrectedRotation(-mLastRotation);
  dxy[0] = shiftX - rMat.xpx * mRefStageX - rMat.xpy * mRefStageY;
  dxy[1] = shiftY - rMat.ypx * mRefStageX - rMat.ypy * mRefStageY;
  numDone = nav->TransformFromRotAlign(mStartingReg, rMat, dxy);
  return numDone;
}

// Disable action buttons if doing tasks: this is called by helper
void CNavRotAlignDlg::Update(void)
{
  BOOL enable = !mWinApp->DoingTasks() && !mWinApp->mCamera->CameraBusy();
  m_butTransform.EnableWindow(enable);
  m_butApplyIS.EnableWindow(enable);
  m_butAlign.EnableWindow(enable);
}
