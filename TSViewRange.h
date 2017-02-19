#pragma once
#include "afxwin.h"
#include "BaseDlg.h"

// CTSViewRange dialog

class CTSViewRange : public CBaseDlg
{
public:
	CTSViewRange(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTSViewRange();

// Dialog Data
	enum { IDD = IDD_TSSETANGLE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual BOOL OnInitDialog();
  virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()
public:
  afx_msg void OnButUseAngle();
  afx_msg void OnButCloseKeep();
  void CloseWindow(bool closeStack);
};
