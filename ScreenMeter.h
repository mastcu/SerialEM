#if !defined(AFX_SCREENMETER_H__13B5EF36_6F96_46F2_B988_64E0547216A4__INCLUDED_)
#define AFX_SCREENMETER_H__13B5EF36_6F96_46F2_B988_64E0547216A4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ScreenMeter.h : header file
//
#include "afxwin.h"
#include "BaseDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CScreenMeter dialog

class CScreenMeter : public CBaseDlg
{
// Construction
public:
  CScreenMeter(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
  //{{AFX_DATA(CScreenMeter)
  enum { IDD = IDD_SCREENMETER };
  CStatic m_statCurrent;
  CString m_strCurrent;
  BOOL  m_bSmoothed;
  //}}AFX_DATA

  CString m_strFontName;


// Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CScreenMeter)
  protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

// Implementation
protected:

  // Generated message map functions
  //{{AFX_MSG(CScreenMeter)
  afx_msg void OnCancel();
  virtual BOOL OnInitDialog();
  afx_msg void OnSmoothed();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()

private:
  CFont mFont;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SCREENMETER_H__13B5EF36_6F96_46F2_B988_64E0547216A4__INCLUDED_)
