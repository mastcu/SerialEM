#pragma once
#include "afxwin.h"


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
};
