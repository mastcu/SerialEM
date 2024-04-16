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
  int RealignReloadedGrid(CMapDrawItem *item, float expectedRot, bool moveInZ, 
    float maxRotation, bool transformNav, CString &errStr);
  void MaintainClosestFour(float delX, float delY, int pci, float radDist[4], int minInd[4],
    int &numClose);
  int MoveStageToTargetPiece();
  void AlignNewReloadImage();
  ScaleMat FindReloadTransform(float dxyOut[2]);
  GetMember(int, DoingMultiGrid);
  GetSetMember(float, RRGMaxRotation);

private:
  CSerialEMApp *mWinApp;
  EMimageBuffer *mImBufs;
  CEMscope *mScope;
  CCameraController *mCamera;
  CShiftManager *mShiftManager;
  CNavHelper *mNavHelper;
  StageMoveInfo mMoveInfo;
  float mRRGMaxRotation;      // Property for default maximum rotation
  bool mAllowOptional;        // Flag to allow the optional characters, bad in shells
  bool mReplaceSpaces;        // Flag to replace spaces
  bool mUseCaretToReplace;    // Flag t use caret insetad of @ when replacing spaces
  int mDoingMultiGrid;        // General flag for one of the routines running
  CArray<JeolCartridgeData, JeolCartridgeData> *mCartInfo;

  // RRG = Realign Reloaded Grid
  KImageStore *mRRGstore;        // Map file
  int mRRGcurStore;              // Pre-existing current store
  MontParam mRRGmontParam;       // Montage param for map
  CMapDrawItem *mRRGitem;        // Map item
  int mRRGpieceSec[5];           // Section numbers in file of each piece
  float mRRGxStages[5], mRRGyStages[5];     // Existing stage positions
  float mRRGnewXstage[5], mRRGnewYstage[5]; // Newly found stage positions
  float mRRGexpectedRot;         // Expected rotation, 0 or big
  float mRRGshiftX, mRRGshiftY;  // Shift to apply after rotation transform
  int mRRGcurDirection;          // Current direction: -1 or 0 for center, 1-4 peripheral 
  ScaleMat mRRGrotMat;           // Rotation matrix for current rotation value
  BOOL mRRGdidSaveState;         // Flag if saved state
  float mRRGangleRange;          // Angle range (2x max) for rotation searches
  bool mRRGtransformNav;         // Flag to transform nav items
  float mRRGbacklashX;           // Backlash to use, from map
  float mRRGbacklashY;
  float mRRGmapZvalue;           // Z to move to from the montage adoc
  bool mBigRotation;             // Flag if a big rotation is expected
  int mMapBuf;                   // Buffer the map is in
  int mMapCenX, mMapCenY;        // Center coordinate of center piece in loaded map
};

