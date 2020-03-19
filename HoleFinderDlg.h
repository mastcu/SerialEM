#pragma once


// CHoleFinderDlg dialog

class CHoleFinderDlg : public CBaseDlg
{
public:
	CHoleFinderDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CHoleFinderDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_HOLE_FINDER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void OnOK();
  virtual void OnCancel();
  virtual BOOL OnInitDialog();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);

	DECLARE_MESSAGE_MAP()
};
