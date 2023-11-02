#pragma once
#include "afxwin.h"


// CSTEMcontrolDlg dialog

class CSTEMcontrolDlg : public CToolDlg
{

public:
	CSTEMcontrolDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSTEMcontrolDlg();

// Dialog Data
	enum { IDD = IDD_STEMCONTROL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

private:
	CFont mFont;
  int mLastProbeMode;
  int mLastSTEMmode;
  BOOL mLastBlanked;
  int mLastMagIndex;

public:
  CButton m_butScreenSwitches;
  BOOL m_bScreenSwitches;
  CButton m_butMatchPixel;
  BOOL m_bMatchPixel;
  afx_msg void OnScreenSwitchStem();
  afx_msg void OnMatchStemPixel();
	virtual BOOL OnInitDialog();
  void UpdateEnables(void);
  void UpdateSettings(void);
  CButton m_butStemMode;
  BOOL m_bStemMode;
  CStatic m_statOnOff;
  afx_msg void OnStemMode();
  void UpdateSTEMstate(int probeMode, int magInd = -1);
  CString m_strOnOff;
  CButton m_butInvertContrast;
  BOOL m_bInvertContrast;
  CEdit m_editAddedRot;
  CString m_strAddedRot;
  afx_msg void OnKillfocusAddedRot();
  afx_msg void OnInvertContrast();
	void OnOK();
  CButton m_butSetISneutral;
  afx_msg void OnSetIsNeutral();
  CButton m_butBlankInStem;
  BOOL m_bBlankInStem;
  CButton m_butRetractToUnblank;
  BOOL m_bRetractToUnblank;
  CButton m_butUnblank;
  afx_msg void OnBlankInStem();
  afx_msg void OnRetractToUnblank();
  afx_msg void OnUnblankStem();
  void BlankingUpdate(BOOL blanked);
};
