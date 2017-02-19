#pragma once
#include "BaseDlg.h"
#include "afxwin.h"

// CThreeChoiceBox dialog

class CThreeChoiceBox : public CBaseDlg
{

public:
	CThreeChoiceBox(CWnd* pParent = NULL);   // standard constructor
	virtual ~CThreeChoiceBox();

// Dialog Data
	enum { IDD = IDD_THREE_CHOICE_BOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
  CBrush mBkgdBrush;
  COLORREF mBkgdColor;

	DECLARE_MESSAGE_MAP()
public:
  CButton m_butOK;
  CButton m_butMiddle;
  CButton m_butCancel;
  afx_msg void OnButMiddle();
  CString mYesText;
  CString mNoText;
  CString mCancelText;
  int mDlgType;
  int mChoice;
  int mSetDefault;     // 1 to set to No, 2 to set to cancel
  CString m_strMessage;
  CStatic m_statMessage;
  CStatic m_statGrayPanel;
};
