#pragma once
#include "afxwin.h"
#include "afxcmn.h"


// CScreenShotDialog dialog

class CScreenShotDialog : public CBaseDlg
{
public:
  CScreenShotDialog(CWnd* pParent = NULL);   // protected constructor, singleton class
  virtual ~CScreenShotDialog();

  // Dialog Data
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_SCREENSHOT };
#endif

protected:

  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void OnOK();
  virtual void OnCancel();
  virtual BOOL OnInitDialog();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);

  DECLARE_MESSAGE_MAP()
public:
  void UpdateSettings();
  void UpdateActiveView();
  void UpdateRunnable();
  static bool GetSnapshotError(int err, CString &report);

private:
  ScreenShotParams mParams;
  ScreenShotParams *mMasterParams;

  void DialogToParams();
  void ParamsToDialog();
  void FixScaleSetSpinner(CSpinButtonCtrl *sbc, float &scale, float minVal);
  void ShowSpinnerValue(CString &str, float scale);
  void ManageEnables();

public:
  int m_iImageScaleType;
  afx_msg void OnImageScaling();
  CStatic m_statScaleBy;
  CString m_strScaleBy;
  CSpinButtonCtrl m_sbcIncreaseZoom;
  BOOL m_bScaleSizes;
  CStatic m_statScaleSizesBy;
  CString m_strScaleSizesBy;
  CSpinButtonCtrl m_sbcScaleSizesBy;
  afx_msg void OnDeltaposSpinScaleSizeBy(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnDeltaposSpinIncreaseZoomBy(NMHDR *pNMHDR, LRESULT *pResult);
  BOOL m_bDrawOverlays;
  int m_iFileType;
  int m_iCompression;
  CStatic m_statCompress;
  CButton m_butNoCompress;
  CButton m_butZipCompress;
  CButton m_butLZWCompress;
  CButton m_butJpegCompress;
  afx_msg void OnFileType();
  afx_msg void OnTakeSnapshot();
  afx_msg void OnCheckScaleSizeBy();
  afx_msg void OnCheckDrawOverlays();
  afx_msg void OnRnocompress();
  afx_msg void OnEnKillfocusJpegQuality();
  CEdit m_editJpegQuality;
  int m_iJpegQuality;
  CSpinButtonCtrl m_sbcJpegQuality;
  afx_msg void OnDeltaposSpinJpegQuality(NMHDR *pNMHDR, LRESULT *pResult);
  CStatic m_statJpegQuality;
  CButton m_butTakeSnapshot;
};
