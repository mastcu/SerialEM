#pragma once
#include "afxwin.h"

#define MAX_DECTRIS_TYPES 6

struct DectrisPlugFuncs;
struct CamPluginFuncs;

struct DectrisThreadData
{
  DectrisPlugFuncs *dectrisPlugFuncs;
  CamPluginFuncs *camPlugFuncs;
  int camIndex;
  CString errString;
};

// CDectrisToolDlg dialog

class CDectrisToolDlg : public CToolDlg
{

public:
	CDectrisToolDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDectrisToolDlg();
  void KillUpdateTimer();
  virtual BOOL OnInitDialog();
  void OnOK();
  bool DoingInitialize() {return mInitializeThread != NULL; };
  GetMember(bool, DoingFlatfield);

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DECTRIS_TOOLDLG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
  CString m_strHVstateValue;
  CString m_strSensorPos;
  CString m_strStatus;
  afx_msg void OnButAcquireFlatfield();
  afx_msg void OnButAdvancedSet();
  afx_msg void OnButInitDetector();
  afx_msg void OnButToggleHv();
  void UpdateEnables();
  CameraParameters *GetDectrisCamParams(int &index);
  int mUpdateInterval;
  void UpdateToolDlg();
  static UINT InitializeProc(LPVOID pParam);
  int InitializeBusy();
  void CleanupInitialize(int error);
  void InitializeDone();
  void FlatfieldNextTask();
  void FlatfieldDone();
  void CleanupFlatfield(int error);


private:
  bool mDoingFlatfield;
  int mNumFlatAcquires;
  int mFlatCount;
  float mFlatExposure;
  UINT_PTR mStatusUpdateID;
  DectrisThreadData mDTD;
  CWinThread *mInitializeThread;

};
