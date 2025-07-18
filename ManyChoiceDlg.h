#pragma once


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

public:
	BOOL m_isRadio;
	int m_numChoices;
	char m_header;
	char m_choiceLabels[15];
	BOOL m_checkboxVals[15];
	BOOL m_radioVals[15];

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
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
};
