#pragma once
#include "afxwin.h"

class CNavHelper;
class CMultiHoleCombiner;

// CMultiCombinerDlg dialog

class CMultiCombinerDlg : public CBaseDlg
{

public:
	CMultiCombinerDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CMultiCombinerDlg();
  void CloseWindow();
  void UpdateSettings();
  void UpdateEnables();

private:
  CNavHelper *mHelper;
  CMultiHoleCombiner *mCombiner;

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MULTI_COMBINER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnCancel();
  virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()
public:
  afx_msg void OnCombineType();
  afx_msg void OnUndoCombine();
  afx_msg void OnCheckDisplayMulti();
  BOOL m_bDisplayMultiShot;
  CButton m_butUndoCombine;
  int m_iCombineType;
  afx_msg void OnCombinePoints();
  CButton m_butCombinePts;
  CButton m_butTurnOffOutside;
  BOOL m_bTurnOffOutside;
  afx_msg void OnTurnOffOutside();
  CString m_strHoleThresh;
  CSpinButtonCtrl m_sbcHoleThresh;
  CButton m_butRemovePoints;
  BOOL m_bRemovePoints;
  CButton m_butTurnOffCombined;
  BOOL m_bTurnOffCombined;
  afx_msg void OnTurnOffCombined();
  afx_msg void OnCheckRemovePoints();
  afx_msg void OnDeltaposSpinHoleThresh(NMHDR *pNMHDR, LRESULT *pResult);
  BOOL m_bSkipAveraging;
  afx_msg void OnCheckSkipAveraging();
};
