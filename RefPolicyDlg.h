#pragma once

#include "BaseDlg.h"
#include "afxwin.h"

// CRefPolicyDlg dialog

class CRefPolicyDlg : public CBaseDlg
{

public:
	CRefPolicyDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CRefPolicyDlg();

// Dialog Data
	enum { IDD = IDD_REFPOLICY };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
  CButton m_butIgnoreHigh;
  BOOL m_bIgnoreHigh;
  CButton m_butPreferBin2;
  BOOL m_bPreferBin2;
  CStatic m_statDMgroup;
  CButton m_butAskNewer;
  CButton m_butAskNever;
  CButton m_butAskAlways;
  BOOL m_iAskPolicy;
  BOOL mAnyGatan;
  BOOL mAnyHigher;
};
