#pragma once


// CMultiCombinerDlg dialog

class CMultiCombinerDlg : public CBaseDlg
{

public:
	CMultiCombinerDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CMultiCombinerDlg();
  void CloseWindow();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MULTI_COMBINER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();
  virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()
};
