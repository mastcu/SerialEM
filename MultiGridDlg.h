#pragma once

#define MAX_MULGRID_PANELS 6
// CMultiGridDlg dialog

class CMultiGridDlg : public CBaseDlg
{

public:
	CMultiGridDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CMultiGridDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MULTI_GRID };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);

	DECLARE_MESSAGE_MAP()

private:
  int mPanelStart[MAX_MULGRID_PANELS];
  int mNumInPanel[MAX_MULGRID_PANELS];

};
