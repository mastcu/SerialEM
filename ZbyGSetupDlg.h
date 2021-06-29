#pragma once


// ZbyGSetupDlg dialog

class CZbyGSetupDlg : public CBaseDlg
{
public:
	CZbyGSetupDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CZbyGSetupDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_Z_BY_G_SETUP };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);

	DECLARE_MESSAGE_MAP()
};
