#pragma once
#include "afxwin.h"
#include "NavHelper.h"


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
  CArray<BaseMarkerShift, BaseMarkerShift> *mMarkerShiftArr;
  CString m_strMarkerShift;
  CString m_strWhatShifts;
  int mFromMag;
  int mToMag;
  int mBaseToMag;
  bool mMapWasShifted;
  bool mOKtoShift;
  int m_iApplyToWhich;
  afx_msg void OnRadioWhichToShift();
  int m_iSaveType;
  CStatic m_statMapMarked;
  afx_msg void OnButRemoveShift();
  CListBox m_listSavedShifts;
  afx_msg void OnSelchangeListSavedShifts();
  void FillListBox();
};
