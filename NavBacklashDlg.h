#pragma once
#include "afxwin.h"
#include "BaseDlg.h"

// CNavBacklashDlg dialog

class CNavBacklashDlg : public CBaseDlg
{

public:
	CNavBacklashDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CNavBacklashDlg();
// Overrides
	virtual void OnOK();
// Dialog Data
	enum { IDD = IDD_NAVBACKLASH };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
  CEdit m_editMinField;
  CStatic m_statMinField;
  CStatic m_statMicrons;
  float m_fMinField;
  int m_iAskOrAuto;
  afx_msg void OnRbacklashNone();
  void ManageFOV(void);
};
