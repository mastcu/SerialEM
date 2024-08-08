#pragma once

#include "MultiGridTasks.h"
#include "afxwin.h"


// CMGSettingsManagerDlg dialog

class CMGSettingsManagerDlg : public CBaseDlg
{

public:
	CMGSettingsManagerDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CMGSettingsManagerDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MULGRID_SETTINGS };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);

	DECLARE_MESSAGE_MAP()


private:
  CMultiGridTasks *mMGTasks;
  CNavHelper *mNavHelper;
  CArray<JeolCartridgeData, JeolCartridgeData> *mCartInfo;
  CArray<MGridMultiShotParams, MGridMultiShotParams> *mMGMultiShotParamArray;
  CArray<MGridHoleFinderParams, MGridHoleFinderParams> *mMGHoleFinderParamArray;
  CArray<MGridAutoContParams, MGridAutoContParams> *mMGAutoContParamArray;
  CArray<MGridGeneralParams, MGridGeneralParams> *mMGGeneralParamArray;
  CArray<MGridFocusPosParams, MGridFocusPosParams> *mMGFocusPosParamArray;
  CArray<MGridAcqItemsParams, MGridAcqItemsParams> *mMGAcqItemsParamArray;
  MGridMultiShotParams *mSavedMultiShotParams;
  MGridHoleFinderParams *mSavedHoleFinderParams;
  MGridAutoContParams *mSavedAutoContParams;
  MGridGeneralParams *mSavedGeneralParams;
  MGridFocusPosParams *mSavedFocusPosParams;
  MGridAcqItemsParams *mSavedAcqItemsParam;
  MGridAcqItemsParams mPreDlgAcqItemsParam;
  bool mDidSavePreDlgParams;


public:
  int mJcdIndex;
  void CloseWindow();
  void SetJcdIndex(int jcdInd);
  afx_msg void OnButGriduseAC();
  afx_msg void OnButApplyAC();
  afx_msg void OnButRestoreCurAC();
  afx_msg void OnButGriduseHF();
  afx_msg void OnButApplyHF();
  afx_msg void OnButRestoreCurHF();
  afx_msg void OnButGriduseMS();
  afx_msg void OnButApplyMS();
  afx_msg void OnButRestoreCurMS();
  afx_msg void OnButGriduseGEN();
  afx_msg void OnButApplyGEN();
  afx_msg void OnButRestoreCurGEN();
  afx_msg void OnButGriduseFP();
  afx_msg void OnButApplyFP();
  afx_msg void OnButRestoreCurFP();
  afx_msg void OnButRevertAC();
  afx_msg void OnButRevertHF();
  afx_msg void OnButRevertMS();
  afx_msg void OnButRevertGEN();
  afx_msg void OnButRevertFP();
  afx_msg void OnButRevertFINAL();
  afx_msg void OnButSetViewFinal();
  afx_msg void OnButApplyFinal();
  afx_msg void OnButRestoreCurFinal();
  void RestorePreDlgParams(int postponed);
  void ManageEnables();
  void UpdateSettings();
  void UpdateAutoContGrid();
  void UpdateAutoContCurrent();
  void UpdateHoleFinderGrid();
  void UpdateHoleFinderCurrent();
  void UpdateMultiShotGrid();
  void UpdateMultiShotCurrent();
  void UpdateGeneralGrid();
  void UpdateGeneralCurrent();
  void UpdateFocusPosGrid();
  void UpdateFocusPosCurrent();
  void UpdateAcqItemsGrid();
  void UpdateAcqItemsCurrent() { ; };
  void FormatAutoContSettings(MGridAutoContParams &param, CString &str);
  void FormatHoleFinderSettings(MGridHoleFinderParams &param, CString &str);
  void FormatMultiShotSettings(MGridMultiShotParams &param, CString &str);
  void FormatGeneralSettings(MGridGeneralParams &param, CString &str);
  void FormatFocusPosSettings(MGridFocusPosParams &param, CString &str);
  CString FormatValueOrNone(float value);
  CStatic m_statACtitle;
  CString m_strAutoContCurrent;
  CString m_strAutoContGrid;
  CString m_strHoleFinderCurrent;
  CString m_strHoleFinderGrid;
  CString m_strMultiShotCurrent;
  CString m_strMultiShotGrid;
  CString m_strGeneralCurrent;
  CString m_strGeneralGrid;
  CString m_strFocusPosCurrent;
  CString m_strFocusPosGrid;
  CStatic m_statHFtitle;
  CStatic m_statMStitle;
  CStatic m_statFinalTitle;
  CStatic m_statGenTitle;
  CStatic m_statFPtitle;
  CStatic m_statGrid1;
  CStatic m_statGrid2;
  CStatic m_statGrid3;
  CStatic m_statGrid4;
  CStatic m_statGrid5;
  CStatic m_statAcqItems;
};
