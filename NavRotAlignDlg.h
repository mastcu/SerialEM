#pragma once
#include "afxwin.h"
#include "BaseDlg.h"

class CNavHelper;
class EMbufferManager;
class CNavigatorDlg;


// CNavRotAlignDlg dialog

class CNavRotAlignDlg : public CBaseDlg
{
public:
	CNavRotAlignDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CNavRotAlignDlg();
  static void InitializeStatics(void);

// Dialog Data
	enum { IDD = IDD_NAVROTALIGN };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
  virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()
public:
  float m_fCenterAngle;
  BOOL m_bSearchAngles;
  CEdit m_editSearchRange;
  float m_fSearchRange;
  afx_msg void OnButAlignToMap();

private:
  double mRotTimeStamp;
  static double mOrigTimeStamp;
  static int mRefMapID;
  static float mRefStageX;
  static float mRefStageY;
  static EMimageBuffer *mImBufs;
  static CNavHelper *mHelper;
  static CSerialEMApp *mWinApp;
  CNavigatorDlg *mNav;
  static EMbufferManager *mBufManager;
  static float mLastRotation;
  int mCurrentReg;
  static int mStartingReg;
  static bool mAppliedIS;

public:
  void UnloadData(void);
  CButton m_butTransform;
  static void PrepareToUseImage(void);
  static int CheckAndSetupReference(void);
  static int AlignToMap(float centerAngle, float range, float &bestRot);
  static int TransformItemsFromAlignment(void);

  CButton m_butApplyIS;
  afx_msg void OnButApplyIs();
  void Update(void);
  CButton m_butAlign;
};
