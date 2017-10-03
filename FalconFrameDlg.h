#pragma once
#include "BaseDlg.h"
#include "afxcmn.h"
#include "ControlSet.h"
#include "afxwin.h"

class CFalconHelper;

// CFalconFrameDlg dialog

class CFalconFrameDlg : public CBaseDlg
{

public:
	CFalconFrameDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CFalconFrameDlg();

// Dialog Data
	enum { IDD = IDD_FALCON_FRAMES };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
  float m_fExposure;
  CString m_strReadouts;
  CSpinButtonCtrl m_sbcNumFrames;
  CSpinButtonCtrl m_sbcSkipStart;
  CSpinButtonCtrl m_sbcTotalSave;
  CSpinButtonCtrl m_sbcSkipAfter;
  afx_msg void OnKillfocusEditExposure();
  afx_msg void OnKillfocusEditReadouts();
  afx_msg void OnDeltaposSpinNumFrames(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnDeltaposSpinSkipStart(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnDeltaposSpinTotalSave(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnDeltaposSpinSkipAfter(NMHDR *pNMHDR, LRESULT *pResult);

  float mReadoutInterval;
  int mMaxFrames;
  int mMaxPerFrame;
  int mNumSkipBefore;
  int mNumSkipAfter;
  ShortVec mSummedFrameList;
  FloatVec mUserFrameFrac;
  FloatVec mUserSubframeFrac;
  float mExposure;
  bool mK2Type;
  bool mAligningInFalcon;
  int mReadMode;
  CameraParameters *mCamParams;

private:
  int mTotalSaved;
  CFalconHelper *mHelper;
  
public:
  void UpdateAllDisplays(void);
  CString m_strSkipStart;
  CString m_strNumFrames;
  CString m_strTotalSave;
  CString m_strSkipEnd;
  CStatic m_statStartLabel;
  CStatic m_statEndLabel;
  CEdit m_editSubframeTime;
  float m_fSubframeTime;
  afx_msg void OnKillfocusSubframeTime();
  CStatic m_statSkipStart;
  CStatic m_statSkipEnd;
  afx_msg void OnButEqualize();
  CStatic m_statExplanation;
};
