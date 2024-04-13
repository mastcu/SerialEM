#pragma once
#include "EMscope.h"

enum { MG_INVENTORY = 1, MG_LOW_MAG_MAP, MG_REALIGN_RELOADED, MG_RRG_MOVED_STAGE, MG_RRG_TOOK_IMAGE };

class CMultiGridTasks
{
public:
  CMultiGridTasks();
  ~CMultiGridTasks();
  void Initialize();
  int ReplaceBadCharacters(CString &str);
  GetSetMember(bool, AllowOptional);
  GetSetMember(bool, ReplaceSpaces);
  GetSetMember(bool, UseCaretToReplace);
  int GetInventory();
  void MultiGridNextTask(int param);
  int MultiGridBusy();
  void CleanupMultiGrid(int error);
  void StopMultiGrid();
  int RealignReloadedGrid(CMapDrawItem *item, float expectedRot, bool moveInZ, CString &errStr);
  void MaintainClosestFour(float delX, float delY, int pci, float radDist[4], int minInd[4],
    int &numClose);
  int MoveStageToTargetPiece();
  ScaleMat FindReloadTransform(float dxyOut[2]);
  GetMember(int, DoingMultiGrid);

private:
  CSerialEMApp * mWinApp;
  EMimageBuffer *mImBufs;
  CEMscope *mScope;
  CCameraController *mCamera;
  CShiftManager *mShiftManager;
  CNavHelper *mNavHelper;
  bool mAllowOptional;
  bool mReplaceSpaces;
  bool mUseCaretToReplace;
  int mDoingMultiGrid;
  CArray<JeolCartridgeData, JeolCartridgeData> *mCartInfo;
  KImageStore *mRRGstore;
  int mRRGcurStore;
  MontParam mRRGmontParam;
  int mRRGpieceSec[5];
  float mRRGxStages[5], mRRGyStages[5];
  float mRRGnewXstage[5], mRRGnewYstage[5];
  float mRRGexpectedRot;
  float mRRGshiftX, mRRGshiftY;
  int mRRGcurDirection;
  ScaleMat mRRGrotMat;
  BOOL mRRGdidSaveState;
  float mRRGangleRange;
  float mRRGbacklashX;
  float mRRGbacklashY;
  float mRRGmapZvalue;
};

