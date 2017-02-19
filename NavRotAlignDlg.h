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
  double mOrigTimeStamp;
  int mRefMapID;
  float mRefStageX;
  float mRefStageY;
  EMimageBuffer *mImBufs;
  CNavHelper *mHelper;
  CNavigatorDlg *mNav;
  EMbufferManager *mBufManager;
  float mLastRotation;
  int mCurrentReg;
  int mStartingReg;
  bool mAppliedIS;

public:
  void UnloadData(void);
  CButton m_butTransform;
  void PrepareToUseImage(void);
  CButton m_butApplyIS;
  afx_msg void OnButApplyIs();
  void Update(void);
  CButton m_butAlign;
};
