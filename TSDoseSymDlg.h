#pragma once
#include "afxwin.h"
#include "TiltSeriesParam.h"

// CTSDoseSymDlg dialog

class CTSDoseSymDlg : public CBaseDlg
{

public:
	CTSDoseSymDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTSDoseSymDlg();
  static void FindDoseSymmetricAngles(TiltSeriesParam &tsParam, FloatVec &idealAngles,
    ShortVec &directions, int &anchorIndex, int &finalPartIndex, CString *summary = NULL);
  static int SetAnglesToUse(TiltSeriesParam &tsParam, FloatVec &angles, CString &mess);
  static void IgnoreAnglesToUseNextCall();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TSDOSESYM };
#endif

protected:
  virtual void OnOK();
  virtual BOOL OnInitDialog();
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
  TiltSeriesParam mTSparam;
  CString m_strGroupSize;
  CSpinButtonCtrl m_sbcGroupSize;
  afx_msg void OnDeltaposSpinGroupSize(NMHDR *pNMHDR, LRESULT *pResult);
  BOOL m_bIncGroup;
  afx_msg void OnCheckIncGroup();
  CEdit m_editIncGroupAngle;
  float m_fIncGroupAngle;
  CStatic m_statIncAngle;
  CStatic m_statIncLabel1;
  CStatic m_statIncLabel2;
  CStatic m_statIncAmount;
  CString m_strIncAmount;
  CSpinButtonCtrl m_sbcIncAmount;
  afx_msg void OnDeltaposSpinIncAmount(NMHDR *pNMHDR, LRESULT *pResult);
  CStatic m_statMinForAnchor;
  CString m_strMinForAnchor;
  CSpinButtonCtrl m_sbcMinForAnchor;
  afx_msg void OnDeltaposSpinMinForAnchor(NMHDR *pNMHDR, LRESULT *pResult);
  BOOL m_bRunToEnd;
  CEdit m_editRunToEnd;
  float m_fRunToEnd;
  afx_msg void OnCheckRunToEnd();
  CString m_strSummary;
  afx_msg void OnEnKillfocusEditIncGroupAngle();
  afx_msg void OnEnKillfocusEditRunToEnd();
  void ManageSummary();
  CButton m_butUseAnchor;
  BOOL m_bUseAnchor;
  afx_msg void OnCheckUseAnchor();
  void ManageEnables();
  CEdit m_editIncInterval;
  int m_iIncInterval;
  CStatic m_statAnchorTilts;
  afx_msg void OnEnKillfocusEditIncInterval();
  CButton m_butReorderFile;
  BOOL m_bReorderFile;
  CString m_strAngleSummary;
  CStatic m_statEvery;
  BOOL m_bSkipBacklash;
  static FloatVec mIdealAnglesToUse;
  static bool mIgnoreAnglesToUse;
};
