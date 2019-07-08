#pragma once
#include "afxwin.h"
#include "ParticleTasks.h"


// CDriftWaitSetupDlg dialog

class CDriftWaitSetupDlg : public CBaseDlg
{

public:
	CDriftWaitSetupDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDriftWaitSetupDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DRIFTWAITSETUP };
#endif

protected:
  virtual void OnOK();
  virtual BOOL OnInitDialog();
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
};
