#pragma once
#include "afxwin.h"
#include "BaseDlg.h"


// CReadFileDlg dialog

class CReadFileDlg : public CBaseDlg
{
public:
	CReadFileDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CReadFileDlg();

// Dialog Data
	enum { IDD = IDD_READFILEDLG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
  virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()
public:
  int m_iSection;
  CButton m_butOK;
  CButton m_butRead;
  CButton m_butNext;
  CButton m_butPrevious;
  afx_msg void OnButReadsec();
  afx_msg void OnButNextSec();
  afx_msg void OnButPrevSec();
  void Update(void);
  void ReadSection(int delta);
  CEdit m_editSection;
};
