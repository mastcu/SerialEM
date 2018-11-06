#pragma once
#include "BaseDlg.h"
#include "afxwin.h"
#include "afxcmn.h"


// CFrameAlignDlg dialog

class CFrameAlignDlg : public CBaseDlg
{

public:
	CFrameAlignDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CFrameAlignDlg();

// Dialog Data
	enum { IDD = IDD_FRAMEALIGN };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()

private:
  int mPanelStart[5];
  int mNumInPanel[5];
  CArray<FrameAliParams, FrameAliParams> *mParams;
  int mPairwiseNum;
  int mAlignBin;
  int mAlignTargetSize;
  int mGroupSize;
  int mRefineIter;
  int mSmoothCrit;
  float mRefineCutoff;
  float mCutoff2, mCutoff3;
  int mNumDMFilters;
  CameraParameters *mCamParams;
  bool mFalconCanAlign, mDEcanAlign;

public:
  int m_iWhereAlign;          // radio button choice
  int mCurParamInd;           // Current parameter index
  int mCurFiltInd;            // Current filter index for DM
  BOOL mMoreParamsOpen;       // If lower panel is open
  int m_iAlignStrategy;       // Strategy choice
  BOOL mGPUavailable;         // Whether GPU is available on K2
  BOOL mServerIsRemote;       // Whether server is remote
  BOOL mNewerK2API;           // If using the new K2 API
  BOOL mUseGpuTransfer[2];    // Whether to use GPU for current camera and offline
  BOOL mEnableWhere;          // Whether the where radio buttons are enabled
  BOOL m_bWholeSeries;        // Whole tilt series checkbox
  int mCameraSelected;        // Which camera is currently selected in cam setup
  int mConSetSelected;        // And which parameters set
  int mReadMode;              // And the current read mode in the dialog
  bool mTakingK3Binned;       // Whether current conset and setting would bin K3 frames

  BOOL m_bUseGPU;
  CStatic m_statPairwiseNum;
  CSpinButtonCtrl m_sbcPairwiseNum;
  CSpinButtonCtrl m_sbcAlignBin;
  CSpinButtonCtrl m_sbcGroupSize;
  CSpinButtonCtrl m_sbcRefineIter;
  CSpinButtonCtrl m_sbcSmoothCrit;
  afx_msg void OnAlignInDM();
  void ManagePanels(void);
  afx_msg void OnButMoreParams();
  CString m_strCurName;
  afx_msg void OnButNewParams();
  BOOL m_bTruncateAbove;
  float m_fTruncation;
  CString m_strPairwiseNum;
  afx_msg void OnPairwiseNum();
  CString m_strAlignBin;
  float m_fCutoff;
  int m_iMaxShift;
  int m_bGroupFrames;
  CStatic m_statGroupSize;
  CString m_strGroupSize;
  CStatic m_statGroupNeeds;
  CString m_strGroupNeeds;
  BOOL m_bRefineAlignment;
  CStatic m_statIterLabel;
  CStatic m_statRefineIter;
  CString m_strRefineIter;
  BOOL m_bSmoothShifts;
  CStatic m_statSmoothLabel;
  CStatic m_statSmoothCrit;
  CString m_strSmoothCrit;
  CButton m_butRefineGroups;
  BOOL m_bRefineGroups;
  CStatic m_statRefineCutoff;
  CEdit m_editRefineCutoff;
  CString m_strRefineCutoff;
  CStatic m_statStopIter;
  CEdit m_editStopIter;
  float m_fStopIter;
  CStatic m_statStopLabel;
  CButton m_butHybridShifts;
  BOOL m_bHybridShifts;
  float m_fSigmaRatio;
  CString m_strCutoff2;
  CString m_strCutoff3;
  afx_msg void OnKillfocusEditCutoff2();
  afx_msg void OnKillfocusEditCutoff3();
  CButton m_butGroupFrames;
  CStatic m_statOtherCuts;
  CEdit m_editCutoff2;
  CEdit m_editCutoff3;
  afx_msg void OnGroupFrames();
  afx_msg void OnRefineAlignment();
  afx_msg void OnSmoothShifts();
  afx_msg void OnDeltaposSpinPairwiseNum(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnDeltaposSpinAlignBin(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnDeltaposSpinGroupSize(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnDeltaposSpinRefineIter(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnDeltaposSpinSmoothCrit(NMHDR *pNMHDR, LRESULT *pResult);
  void ManageGroupSizeMinFrames(void);
  void ManageAlignStategy(void);
  void ManageRefinement(void);
  void ManageGrouping(void);
  void ManageSmoothing(void);
  CStatic m_statFrameLabel;
  afx_msg void OnEnKillfocusEditRefineCutoff();
  bool ProcessBlankIsZeroEditBox(CString & editStr, float &cutoff);
  void SetBlankIsZeroEditBox(CString & editStr, float &cutoff);
  void LoadParamToDialog(void);
  void UnloadDialogToParam(int index);
  void ManageTruncation(void);
  CEdit m_editTruncation;
  CStatic m_statCountLabel;
  afx_msg void OnTruncateAbove();
  CComboBox m_comboFilter;
  void UnloadCurrentPanel(int whereAlign);
  CListBox m_listSetNames;
  afx_msg void OnSelchangeListSetNames();
  afx_msg void OnButDeleteSet();
  CButton m_butDeleteSet;
  afx_msg void OnEnChangeEditName();
  CButton m_butUseGPU;
  CButton m_butSetFolder;
  afx_msg void OnButSetFolder();
  CButton m_butKeepPrecision;
  BOOL m_bKeepPrecision;
  CButton m_butSaveFloatSums;
  BOOL m_bSaveFloatSums;
  CButton m_butWholeSeries;
  CButton m_butAlignSubset;
  BOOL m_bAlignSubset;
  afx_msg void OnAlignSubset();
  CStatic m_statSubsetTo;
  CStatic m_statSubsetParen;
  CEdit m_editSubsetStart;
  CEdit m_editSubsetEnd;
  int m_iSubsetStart;
  int m_iSubsetEnd;
  BOOL m_bOnlyInPlugin;
  BOOL m_bOnlyWithIMOD;
  BOOL m_bOnlyNonSuperRes;
  BOOL m_bOnlySuperRes;
  afx_msg void OnOnlyInPlugin();
  afx_msg void OnOnlyWithImod();
  afx_msg void OnOnlyNonSuper();
  afx_msg void OnOnlySuperresolution();
  afx_msg void OnKillfocusEditSubsetStart();
  afx_msg void OnKillfocusEditSubsetEnd();
  int CheckConditionsOnClose(int whereAlign, int curIndex, int & newIndex);
  CButton m_butUseFrameFolder;
  BOOL m_bUseFrameFolder;
  afx_msg void OnUseFrameFolder();
  CButton m_butTruncateAbove;
  CStatic m_statAlignTarget;
  CString m_strAlignTarget;
  afx_msg void OnAliBinByFac();
  CStatic m_statAlibinPix;
  CSpinButtonCtrl m_sbcBinTo;
  afx_msg void OnDeltaposSpinBinTo(NMHDR *pNMHDR, LRESULT *pResult);
  int m_iBinByOrTo;
  void ManageAlignBinning(void);
  CStatic m_statAlignBin;
};
