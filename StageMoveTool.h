#pragma once


// CStageMoveTool dialog

#pragma once
#include "afxwin.h"
#include "BaseDlg.h"

class CStageMoveTool : public CBaseDlg
{
public:
	CStageMoveTool(CWnd* pParent = NULL);   // standard constructor
	virtual ~CStageMoveTool();

// Dialog Data
	enum { IDD = IDD_STAGEMOVETOOL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
  virtual void PostNcDestroy();
  afx_msg void OnPaint();
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);

	DECLARE_MESSAGE_MAP()

private:
  int mUnitRadius;
  int mXcenter, mYcenter;
  int mOuterRadius;
  int mInnerRadius;
  float mOuterFrames;  // # of unit frames to move at outer radius
  double mDelStageX, mDelStageY;
  double mLastUpdateX, mLastUpdateY;
  int mLastImageSet;
  int mLastBusy;
  BOOL mGoingToAcquire;
  CCameraController *mCamera;

public:
  void Update(void);

  CString m_strStageX;
  CString m_strStageY;
  void UpdateStage(double stageX, double stageY);
  bool TooBusyToMove(void);
  void StartStageMove(void);
  void StageToolNextTask(void);
  GetMember(BOOL, GoingToAcquire);
  CStatic m_statStageX;
  CButton m_butImageAfterMove;
  BOOL m_bImageAfterMove;
  void StopNextAcquire(void);
  afx_msg void OnImageAfterMove();
};
