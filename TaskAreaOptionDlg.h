#pragma once


// CTaskAreaOptionDlg dialog

class CTaskAreaOptionDlg : public CBaseDlg
{

public:
	CTaskAreaOptionDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTaskAreaOptionDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TASK_AREA_OPTIONS };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();

	DECLARE_MESSAGE_MAP()
public:
  BOOL m_bUseViewNotSearch;
  afx_msg void OnCheckUseView();
  void ManageVorSlabels();
  int m_iRoughLM;
  int m_iRefine;
  int m_iResetIS;
  int m_iWalkup;
  afx_msg void OnRrefineTrial();
  afx_msg void OnResetisTrial();
};
