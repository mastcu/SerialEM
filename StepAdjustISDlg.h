#pragma once
#include "BaseDlg.h"
#include "afxwin.h"
#include "afxcmn.h"

struct MultiShotParams;

// CStepAdjustISDlg dialog

class CStepAdjustISDlg : public CBaseDlg
{

public:
	CStepAdjustISDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CStepAdjustISDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_STEP_AND_ADJUST_IS };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();

	DECLARE_MESSAGE_MAP()

public:
  int mOtherMagInd;
  int mDefocusOffset;
  int m_iAreaToUse;
  MultiShotParams *mParam;
  int mLowestM;
  int mPrevMag;
  BOOL mHexGrid;
  BOOL mUseCustom;
  BOOL mStartRoutine;
  afx_msg void OnAreaToUse();
  int m_iMagTypeToUse;
  afx_msg void OnMagTypeToUse();
  CStatic m_statOtherMag;
  CString m_strOtherMag;
  CSpinButtonCtrl m_sbcOtherMag;
  afx_msg void OnDeltaposSpinOtherMag(NMHDR *pNMHDR, LRESULT *pResult);
  BOOL m_bSetDefOffset;
  CString m_strDefOffset;
  CSpinButtonCtrl m_sbcDefOffset;
  afx_msg void OnDeltaposSpinDefOffset(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnCheckSetDefOffset();
  void ManageEnables();
  CButton m_butImageAfterIS;
  BOOL m_bImageAfterIS;
  BOOL m_bDoAutomatic;
  afx_msg void OnStaaDoAutomatic();
  int m_iXCorrOrHoleCen;
  afx_msg void OnRadioCrosscorr();
  float m_fHoleSize;
  afx_msg void OnKillfocusEditStaaHole();
  int m_iAutoMethod;
  float m_fShiftLimit;
  afx_msg void OnKillfocusEditStaaLimit();
  afx_msg void OnButStaaStart();
  BOOL m_bSetPrevExp;
  afx_msg void OnCheckSetPrevExp();
  float m_fPrevExposure;
  afx_msg void OnKillfocusEditPreviewExp();
};
