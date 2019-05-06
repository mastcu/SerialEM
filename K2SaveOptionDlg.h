#pragma once


// CK2SaveOptionDlg dialog

#pragma once
#include "BaseDlg.h"
#include "afxwin.h"
#include "afxcmn.h"

class CK2SaveOptionDlg : public CBaseDlg
{
public:
	CK2SaveOptionDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CK2SaveOptionDlg();

// Dialog Data
	enum { IDD = IDD_K2_SAVE_OPTIONS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
  BOOL m_bOneFramePerFile;
  int m_iFileType;
  BOOL m_bPackRawFrames;
  CButton m_butSkipRotFlip;
  BOOL m_bSkipRotFlip;
  BOOL mEnableSkipRotFlip;
  CameraParameters *mCamParams;   // Why wasn't this put here a lot sooner?
  int mFalconType;
  int mCamFlags;
  BOOL mDEtype;
  int mK2Type;
  int mNameFormat;
  int mNumberDigits;
  BOOL mCanCreateDir;
  BOOL mCan4BitModeAndCounting;
  BOOL mCanSaveTimes100;
  BOOL mCanReduceSuperres;
  BOOL mCanUseExtMRCS;
  BOOL mCanSaveFrameStackMdoc;
  BOOL mCanGainNormSum;
  BOOL mSetIsGainNormalized;
  BOOL mTakingK3Binned;
  int mSaveSetting;
  int mAlignSetting;
  int mUseFrameAlign;
  int mK2mode;

private:
  int mPanelStart[5];
  int mNumInPanel[5];

public:
  CString m_strBasename;
  BOOL m_bRootFolder;
  BOOL m_bSavefileFolder;
  BOOL m_bNavLabelFolder;
  BOOL m_bRootFile;
  BOOL m_bSavefileFile;
  BOOL m_bNavLabelFile;
  BOOL m_bNumber;
  BOOL m_bMonthDay;
  BOOL m_bHourMinSec;
  afx_msg void OnKillfocusEditBasename();
  afx_msg void OnCheckComponent();
  void UpdateFormat(void);
  CString m_strExample;
  CStatic m_statMustExist;
  CStatic m_statMustExist2;
  int m_iStartNumber;
  BOOL m_bOnlyWhenAcquire;
  BOOL m_bTiltAngle;
  CButton m_butRootFolder;
  CButton m_butSaveFileFolder;
  CButton m_butNavlabelFolder;
  CButton m_butMonthDay;
  CButton m_butHourMinSec;
  CStatic m_statUseComp;
  CStatic m_statFolder;
  CStatic m_statFile;
  CStatic m_statDigits;
  CString m_strDigits;
  CSpinButtonCtrl m_sbcDigits;
  afx_msg void OnDeltaposSpinDigits(NMHDR *pNMHDR, LRESULT *pResult);
  CButton m_butPackCounting4Bits;
  BOOL m_bPackCounting4Bit;
  CButton m_butUse4BitMode;
  BOOL m_bUse4BitMode;
  void ManagePackOptions(void);
  afx_msg void OnPackRawFrame();
  afx_msg void OnSaveMrc();
  BOOL m_bSaveTimes100;
  CButton m_butUseExtensionMRCS;
  BOOL m_bUseExtensionMRCS;
  CButton m_butSaveFrameStackMdoc;
  BOOL m_bSaveFrameStackMdoc;
  CButton m_butSaveUnnormalized;
  BOOL m_bSaveUnnormalized;
  afx_msg void OnSaveUnnormalized();
  CButton m_butPackRawFrame;
  CButton m_butSavesTimes100;
  CButton m_butReduceSuperres;
  BOOL m_bReduceSuperres;
  afx_msg void OnReduceSuperres();
  CString m_strCurSetSaves;
  CButton m_butDatePrefix;
  BOOL m_bDatePrefix;
  afx_msg void OnCheckDatePrefix();
};
