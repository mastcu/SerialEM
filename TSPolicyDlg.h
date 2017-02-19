#pragma once

#include "BaseDlg.h"
#include "afxwin.h"
class CTSController;

// CTSPolicyDlg dialog

class CTSPolicyDlg : public CBaseDlg
{

public:
	CTSPolicyDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTSPolicyDlg();

// Dialog Data
	enum { IDD = IDD_TSPOLICIES };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
  int m_iTiltFail;
  int m_iDimRecord;
  int m_iRecordLoss;
  int m_iErrorMessage;
  BOOL m_bAutoNewLog;
  CButton m_butMBValves;
  BOOL m_bMBValves;
  afx_msg void OnCheckMbValves();
  CEdit m_editMBMinutes;
  float m_fMBMinutes;
  CButton m_butTiltTerm;
  CButton m_butTiltCont;
  CButton m_butLDlossRepeat;
  CButton m_butLDlossCont;
  CButton m_butLDlossTerm;
  CButton m_butLDdimRepeat;
  CButton m_butLDdimCont;
  CButton m_butLDdimTerm;
  CButton m_butDimTermNotBox;
  CButton m_butDimDoMessage;
  int m_iDimTermNotBox;
  afx_msg void OnErrorMessage();
  void ManagePolicies(void);
  CTSController *mTSController;
  CStatic m_statTiltFail;
  CStatic m_statDimTermNotBox;
  CStatic m_statLDloss;
  CStatic m_statLDdim;
  BOOL m_bEndBidir;
  CEdit m_editDimEndsAbsAngle;
  CEdit m_editBidirEndDiff;
  int m_iDimEndsAbsAngle;
  int m_iDimEndsAngleDiff;
  afx_msg void OnCheckEndBidir();
  CStatic m_statMaxDelta1;
  CStatic m_statMaxDelta2;
  CButton m_butEndHighExp;
  BOOL m_bEndHighExp;
  CEdit m_editMaxDeltaExp;
  float m_fMaxDeltaExp;
  afx_msg void OnCheckEndHighExp();
  void ManageEndBidir(void);
};
