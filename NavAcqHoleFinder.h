#pragma once
#include "afxwin.h"
#include "BaseDlg.h"

// NavAcqHoleFinder dialog

class NavAcqHoleFinder : public CBaseDlg
{

public:
	NavAcqHoleFinder(CWnd* pParent = NULL);   // standard constructor
	virtual ~NavAcqHoleFinder();

  // Overrides
  virtual void OnOK();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_NAVACQ_HOLE_FINDER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
  BOOL m_bRunCombiner;
  afx_msg void OnRunCombiner();
  afx_msg void OnOpenHoleFinder();
  afx_msg void OnOpenCombiner();
};
