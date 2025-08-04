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
  BOOL mCheckboxVals[MAX_CHOICES];
  int mRadioVal;
  int mClickedOK;

  afx_msg void OnRadio(UINT nID);
  afx_msg void OnCheck(UINT nID);
};
