#pragma once
#include "afxwin.h"
#include "setdpi.h"

// CMacroSelector dialog

class CMacroSelector : public CDialog
{
public:
	CMacroSelector(CWnd* pParent = NULL);   // standard constructor
	virtual ~CMacroSelector();

// Dialog Data
	enum { IDD = IDD_MACROSELECTER };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();

	DECLARE_MESSAGE_MAP()

private:
  CSetDPI mSetDPI;


public:
  int mMacroIndex;
  CString m_strInfoText;
  CString m_strEntryText;
  CComboBox m_comboBox;
  bool mAddNone;
  afx_msg void OnCbnSelendokComboSelector();
};
