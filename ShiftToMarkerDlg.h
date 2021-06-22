#pragma once
#include "afxwin.h"


// CShiftToMarkerDlg dialog

class CShiftToMarkerDlg : public CBaseDlg
{

public:
	CShiftToMarkerDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CShiftToMarkerDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SHIFT_TO_MARKER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();

	DECLARE_MESSAGE_MAP()
public:
  CString m_strMarkerShift;
  CString m_strWhatShifts;
  int mFromMag;
  int mToMag;
  int mBaseToMag;
  bool mMapWasShifted;
  int m_iApplyToWhich;
  afx_msg void OnRadioWhichToShift();
  int m_iSaveType;
  CStatic m_statMapMarked;
};
