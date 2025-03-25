#pragma once
#include "afxwin.h"


// CCtfAcquireParamDlg dialog

class CCtfAcquireParamDlg : public CBaseDlg
{

public:
	CCtfAcquireParamDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CCtfAcquireParamDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CTF_ACQ_PARAMS };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();

	DECLARE_MESSAGE_MAP()
public:
  BOOL m_bSetExposure;
  afx_msg void OnCheckCtfExposure();
  float m_fExposure;
  BOOL m_bSetDrift;
  afx_msg void OnCheckCtfDrift();
  float m_fDrift;
  afx_msg void OnEnKillfocusEditCtfExposure();
  BOOL m_bSetBinning;
  afx_msg void OnCheckCtfBinning();
  int m_iBinning;
  BOOL m_bUseFullField;
  BOOL m_bUseFocusInLD;
  BOOL m_bAlignFrames;
  int m_iNumFrames;
  afx_msg void OnCheckctfAlignFrames();
  void ManageEnables();
  float m_fMinFocus;
  float m_fMinAdd;
  BOOL m_bUseAliSet;
  afx_msg void OnCheckUseAliSet();
  int m_iAlignSet;
  afx_msg void OnEnKillfocusEditAlignSet();
  void ShowAlignSetName();
};
