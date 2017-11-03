#pragma once

// CDERefMakerDlg dialog

#include "BaseDlg.h"
#include "afxwin.h"
#include "afxcmn.h"
#include "GainRefMaker.h"

class CDERefMakerDlg : public CBaseDlg
{

public:
	CDERefMakerDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDERefMakerDlg();

// Dialog Data
	enum { IDD = IDD_DE_REFERENCE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
  CameraParameters *mCamParams;
  int mRepeatList[MAX_DE_REF_TYPES];
  float mExposureList[MAX_DE_REF_TYPES];
  int mCurListInd;
  int m_iProcessingType;
  afx_msg void OnProcessingType();
  afx_msg void OnReferenceType();
  int m_iReferenceType;
  CString m_strRefFPS;
  BOOL m_bUseHardwareBin;
  float m_fExposureTime;
  CSpinButtonCtrl m_spinNumRepeats;
  afx_msg void OnDeltaposSpinNumRepeats(NMHDR *pNMHDR, LRESULT *pResult);
  int m_iNumRepeats;
  CButton m_butDarkRef;
  void LoadListItemsToDialog(void);
};
