#pragma once


// CCtffindParamDlg dialog

class CCtffindParamDlg : public CBaseDlg
{

public:
	CCtffindParamDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CCtffindParamDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CTFFINDDLG };
#endif

protected:
  virtual void OnOK();
  virtual void OnCancel();
  virtual BOOL OnInitDialog();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
  afx_msg void OnCheckDrawRings();
  BOOL m_bSlowSearch;
  float m_fCustomMaxRes;
  int m_iMinPhase;
  int m_iMaxPhase;
  BOOL m_bFindPhase;
  BOOL m_bFixAstigForPhase;
  BOOL m_bExtraStats;
  BOOL m_bDrawRings;
  afx_msg void OnCheckSlowSearch();
  afx_msg void OnKillfocusEditMaxFitRes();
  afx_msg void OnKillfocusEditPhaseMinRes();
  afx_msg void OnCheckFindPhase();
  afx_msg void OnCheckFixAstigForPhase();
  afx_msg void OnCheckExtraStats();

private:
  CProcessImage *mProcessImage;
public:
  float m_fPhaseMinRes;
  afx_msg void OnEnKillfocusEditMinPhase();
  afx_msg void OnEnKillfocusEditMaxPhase();
  CString m_strDfltMaxRes;
  void UpdateSettings();
  void RefitToCurrentFFT();
  afx_msg void OnKillfocusEditFixedPhase();
  float m_fFixedPhase;
};
