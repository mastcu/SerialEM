#if !defined(AFX_DISTORTIONTASKS_H__300877E4_E742_43CA_A2DE_AA0F3C782673__INCLUDED_)
#define AFX_DISTORTIONTASKS_H__300877E4_E742_43CA_A2DE_AA0F3C782673__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DistortionTasks.h : header file
//
#include "EMScope.h"



/////////////////////////////////////////////////////////////////////////////
// CDistortionTasks command target

class CDistortionTasks : public CCmdTarget
{
	//DECLARE_DYNCREATE(CDistortionTasks)
public:
	CDistortionTasks();           // protected constructor used by dynamic creation
	~CDistortionTasks();

// Attributes
public:

// Operations
public:
	void SetOverlaps();
	void SPDoFirstMove();
  BOOL DoingStagePairs() {return mIterCount > 0;};
	void StopStagePairs();
	void SPCleanUp(int error);
	static void TaskSPError(int error);
	static void TaskSPDone(int param);
	static int TaskSPBusy();
	void SPNextTask(int param);
	void Initialize();
  void CalibrateDistortion();
  GetSetMember(int, StageDelay);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDistortionTasks)
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDistortionTasks)
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

private:
  CSerialEMApp * mWinApp;
  ControlSet * mConSets;
  EMimageBuffer *mImBufs;
  CEMscope *mScope;
  CCameraController *mCamera;
  CShiftManager *mShiftManager;
  MagTable *mMagTab;

  int mIterLimit;                 // Iteration limit
  int mIterCount;                 // iteration count
  float mStageBacklash;           // Amount to move for backlash correction
  float mBacklashX, mBacklashY;   // AMounts to move in X and Y for this direction
  int mDirectionIndex;             // Direction 0-7
  float mDiagonalMinOverlap;       // Min and max overlap in diagonal cases
  float mDiagonalMaxOverlap;
  float mMajorDimMinOverlap;       // Min and max overlap in side-to-side cases
  float mMajorDimMaxOverlap;
  float mMinorDimMinOverlap;       // Minimum overlap in cross-dimension
  float mTargetMoveX, mTargetMoveY; // Target move in stage coordinates
  float mStageMoveX, mStageMoveY;   // Amount stage was moved in this trial
  StageMoveInfo mCenterInfo;        // Starting stage position
  StageMoveInfo mMoveInfo;          // working copy of moving stage
  float mCumMoveDiffX, mCumMoveDiffY;  // Sum of differences between stage and actual move
  float mMinShiftX, mMaxShiftX;     // Allowed range of shifts between images
  float mMinShiftY, mMaxShiftY;
  int mBinning;
  BOOL mFinishing;
  ScaleMat mAInv;
  BOOL mSaveAlignOnSave;
  FloatVec mPredictedMoveX;         // Adjusted move predicted by error on one run
  FloatVec mPredictedMoveY;
  int mStageDelay;                  // Stage delay to apply if nonzero
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DISTORTIONTASKS_H__300877E4_E742_43CA_A2DE_AA0F3C782673__INCLUDED_)
