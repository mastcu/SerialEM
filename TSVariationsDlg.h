#pragma once
#include "BaseDlg.h"
#include "afxwin.h"
#include "TiltSeriesParam.h"
#include "afxcmn.h"

struct TiltSeriesParam;


// CTSVariationsDlg dialog

class CTSVariationsDlg : public CBaseDlg
{

public:
	CTSVariationsDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTSVariationsDlg();

// Dialog Data
	enum { IDD = IDD_TSVARIATIONS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
  afx_msg void OnSize(UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()


private:
  BOOL mInitialized;
  int m_iBorderY;
  int m_iBorderX;
  int m_iOKoffset;
  int m_iOKLeft;
  int m_iCancelLeft;
  int m_iHelpLeft;
  int m_iHelpOffset;


public:
  CButton m_butOK;
  CButton m_butCancel;
  CListBox m_listViewer;
  BOOL m_bPlusMinus;
  CEdit m_editAngle;
  float m_fAngle;
  CButton m_butPlusMinus;
  CEdit m_editValue;
  float m_fValue;
  int m_iType;
  int m_iLinear;
  TiltSeriesParam *mParam;
  VaryInSeries mVaries[MAX_TS_VARIES];
  int mNumVaries;
  int mCurrentItem;
  BOOL mFilterExists;
  BOOL mK2Selected;
  BOOL mSTEMmode;
  BOOL mDoingTS;
  int mSeriesPower;
  double mTopAngle;

  void Update(void);
  void UpdateListString(int index);
  void ItemToListString(int index, CString &string);
  void FillListBox(void);
  CButton m_butExposure;
  CButton m_butDrift;
  CButton m_butFrameTime;
  CButton m_butFocus;
  CButton m_butLoss;
  CButton m_butSlit;
  CButton m_butFixed;
  CButton m_butLinear;
  void UnloadToCurrentItem(void);
  afx_msg void OnKillfocusEditTsvAngle();
  afx_msg void OnKillfocusEditTsvValue();
  afx_msg void OnCheckTsvPlusMinus();
  afx_msg void OnTsvExposure();
  afx_msg void OnTsvFixedValue();
  afx_msg void OnButTsvAddItem();
  afx_msg void OnButTsvRemove();
  afx_msg void OnSelchangeListTsvTable();
  CButton m_butAddItem;
  CButton m_butRemove;
  static bool PurgeVariations(VaryInSeries *varies, int &numVaries, int &currentItem);
  static void SortVariations(VaryInSeries *varies, int &numVaries, int &currentItem);
  static bool ListVaryingTypes(VaryInSeries *varyArray, int numVaries, int * typeVaries,
    float * zeroDegValues);
  static bool FindValuesToSet(VaryInSeries *varyArray, int numVaries, 
    float *zeroDegValues, float angle, int *typeVaries, float *outValues);
  bool CheckItemValue(bool update);
  float m_fSeriesStep;
  CString m_strPower;
  CSpinButtonCtrl m_sbcSeriesPower;
  CButton m_butAddSeries;
  afx_msg void OnButTsvAddSeries();
  afx_msg void OnDeltaposSpinTsvSeriesPower(NMHDR *pNMHDR, LRESULT *pResult);
  CEdit m_editSeriesStep;
  CButton m_butNumFamesFixed;
  BOOL m_bNumFramesFixed;
  afx_msg void OnNumFramesFixed();
  void AddOneVariation(int type, float angle, float value);
};
