#pragma once
#include "afxwin.h"
#include "BaseDlg.h"

// CNavFileTypeDlg dialog

class CNavFileTypeDlg : public CBaseDlg
{

public:
	CNavFileTypeDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CNavFileTypeDlg();
// Overrides
	virtual void OnOK();

// Dialog Data
	enum { IDD = IDD_NAVFILETYPE};

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
  int m_iSingleMont;
  CButton m_butFitPoly;
  BOOL m_bFitPoly;
  BOOL mPolyFitOK;
  afx_msg void OnRopenForSingle();
  CButton m_butSkipDlgs;
  BOOL m_bSkipDlgs;
};
