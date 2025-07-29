#pragma once

#define MAX_CHOICES 15

// CManyChoiceDlg dialog

class CManyChoiceDlg : public CBaseDlg
{

public:
  CManyChoiceDlg(CWnd* pParent = NULL);   // standard constructor
  virtual ~CManyChoiceDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MANYCHOICEBOX };
#endif

protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();
  DECLARE_MESSAGE_MAP()

private:
  int mPanelStart[2];
  int mNumInPanel[2];

public:
  int mNumChoices;
  bool mIsRadio;	
  CString mHeader;
  CString mChoiceLabels[MAX_CHOICES];
  BOOL m_checkboxVals[MAX_CHOICES];
  CButton m_checkboxes[MAX_CHOICES];
  int m_radioVal;
  CButton m_radios[MAX_CHOICES];
  int mClickedOK;

  afx_msg void OnBnClickedGenericRadio1();
  afx_msg void OnBnClickedGenericRadio2();
  afx_msg void OnBnClickedGenericRadio3();
  afx_msg void OnBnClickedGenericRadio4();
  afx_msg void OnBnClickedGenericRadio5();
  afx_msg void OnBnClickedGenericRadio6();
  afx_msg void OnBnClickedGenericRadio7();
  afx_msg void OnBnClickedGenericRadio8();
  afx_msg void OnBnClickedGenericRadio9();
  afx_msg void OnBnClickedGenericRadio10();
  afx_msg void OnBnClickedGenericRadio11();
  afx_msg void OnBnClickedGenericRadio12();
  afx_msg void OnBnClickedGenericRadio13();
  afx_msg void OnBnClickedGenericRadio14();
  afx_msg void OnBnClickedGenericRadio15();
  afx_msg void OnBnClickedGenericCheck1();
  afx_msg void OnBnClickedGenericCheck2();	
  afx_msg void OnBnClickedGenericCheck3();
  afx_msg void OnBnClickedGenericCheck4();
  afx_msg void OnBnClickedGenericCheck5();
  afx_msg void OnBnClickedGenericCheck6();
  afx_msg void OnBnClickedGenericCheck7();
  afx_msg void OnBnClickedGenericCheck8();
  afx_msg void OnBnClickedGenericCheck9();
  afx_msg void OnBnClickedGenericCheck10();
  afx_msg void OnBnClickedGenericCheck11();
  afx_msg void OnBnClickedGenericCheck12();
  afx_msg void OnBnClickedGenericCheck13();
  afx_msg void OnBnClickedGenericCheck14();
  afx_msg void OnBnClickedGenericCheck15();
  afx_msg void DoCancel();
};
