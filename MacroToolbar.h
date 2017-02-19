#pragma once
#include "afxwin.h"
#include "BaseDlg.h"


// CMacroToolbar dialog

class CMacroToolbar : public CBaseDlg
{
public:
	CMacroToolbar(CWnd* pParent = NULL);   // standard constructor
	virtual ~CMacroToolbar();
  BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );

// Dialog Data
	enum { IDD = IDD_MACROTOOLBAR };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();
  afx_msg void OnCancel();
  virtual BOOL OnInitDialog();
  afx_msg void OnMacroRun(UINT nID);

	DECLARE_MESSAGE_MAP()

private:
  int mWinWidthOrig, mButWidthOrig;
  int mButHeightOrig;
  int mGutterHeight;
  int mLeftPos, mTopPos;
  int mBaseWinHeight;

public:
  void UpdateSettings(void);
  void Update(void);
  void SetOneMacroLabel(int num);
  void SetLength(int num, int butHeight);
};
