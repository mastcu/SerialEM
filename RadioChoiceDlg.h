#pragma once


// CRadioChoiceDlg dialog

class CRadioChoiceDlg : public CDialog
{

public:
	CRadioChoiceDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CRadioChoiceDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_RADIO_CHOICE };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();

	DECLARE_MESSAGE_MAP()
public:
  int m_iChoice;
  CString mInfoLine1, mInfoLine2;
  CString mChoiceOne, mChoiceTwo, mChoiceThree;

};
