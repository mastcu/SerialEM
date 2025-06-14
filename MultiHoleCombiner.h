#pragma once

#include <set>

class CMapDrawItem;
class HoleFinder;
class CNavHelper;
class CNavigatorDlg;

enum { COMBINE_ON_IMAGE = 0, COMBINE_IN_POLYGON, COMBINE_IN_GROUP };

struct PositionData
{
  short xCen, yCen;
  short useXcen, useYcen;
  short startX, endX;
  short startY, endY;
  short numAcquires;
  short cenMissing;
};

class CMultiHoleCombiner
{
public:
  CMultiHoleCombiner(void);
  ~CMultiHoleCombiner(void);

  int CombineItems(int boundType, BOOL turnOffOutside, int inXholes = -9, int inYholes = -9, CMapDrawItem *vecItem = NULL);
  void AddItemToCenters(FloatVec &xCenters, FloatVec &yCenters, IntVec &navInds,
    CMapDrawItem *item, int ind, int &drawnOnID);
  bool OKtoUndoCombine(void);
  void UndoCombination(void);
  bool IsItemInUndoList(int mapID);
  const char *GetErrorMessage(int error);
  MapItemArray *GetPreCombineHoles() { return &mPreCombineHoles; };
  void ClearSavedItemArray(bool originalToo, bool updateDlg);

private:
  CNavHelper *mHelper;
  CSerialEMApp *mWinApp;
  CNavigatorDlg *mNav;
  HoleFinder *mFindHoles;
  EMimageBuffer *mImBuf;          // The associate image buffer if one can be found
  bool mUseImageCoords;           // Flag that analysis is being done with image coord
  ScaleMat mBITSmat;              // Matrix for image to stage
  float mBSTIdelX, mBSTIdelY;     // Delta value from stage to image
  int mNxGrid, mNyGrid;           // Size of grid points seem to fall on
  int mNumXholes, mNumYholes;     // Number of holes in current pattern
  int **mGrid;                    // Line pointers to grid array
  MapItemArray mSavedItems;       // Items removed from Nav array
  MapItemArray mPreCombineHoles;    // All items that have been cleared out
  IntVec mIDsForUndo;             // IDs of the multi-shot items added
  IntVec mIndexesForUndo;         // Their indexes: for rapid checking, can be changed
  IntVec mOrphanIDsToUndo;        // ID's of items turned off because not assigned
  std::set<int> mSetOfUndoIDs;    // A set so they can be looked up easily
  std::set<int> mGroupIDsInPoly;  // The group ID's of items in polygon
  std::set<int> mIDsOutsidePoly;  // IDs of ones turned off
  ScaleMat mSkipXform;            // Transform to get from stage to IS space skip list
  bool mTransposeSize;            // Flag to transpose sizes from stage to IS space
  IntVec mHexDelX, mHexDelY;      // Lists of indexes in the hexagon
  IntVec mOneDIndexToHexRing;     // Maps from a 1D index of hole number in rect pattern
  IntVec mOneDIndexToPosInRing;   // to rung number and position
  int mNumRings;                  // Number of rings in hex pattern
  IntVec mHexToItemIndex;         // Map from hex number to item number as array is built
  int mDebug;

  void TryBoxStartsOnLine(int otherStart, bool doCol,
    CArray<PositionData, PositionData> &fullArray);
  void EvaluateLineOfBoxes(int xStart, int yStart, bool doCol,
    CArray<PositionData, PositionData> &posArray, float &sdOfNums, int &cenMissing);
  void EvaluateBoxAtPosition(int xStart, int yStart, PositionData &data);
  void EvaluateCrossAtPosition(int xCen, int yCen, PositionData &data);
  int EvaluateArrangementOfHexes(int xCen, int yCen,  bool pitchDown, 
    CArray<PositionData, PositionData> &posArray, float &sdOfNums, int &cenMissing);
  void EvaluateHexAtPosition(int xCen, int yCen, PositionData &data, IntVec *boxAssigns);
  void AddPointsToHexItem(MapItemArray *itemArray, PositionData &data, int hex, IntVec &boxAssigns,
    IntVec &navInds, FloatVec &xCenters, FloatVec &yCenters, ScaleMat gridMat, IntVec &ixSkip, IntVec &iySkip, int &numAdded);

  void mergeBoxesAndEvaluate(CArray<PositionData, PositionData> &posArray, float &sdOfNums);
  void SetBoxAcquireLimits(int start, int end, int numHoles, int nGrid, int &acqStart, int &acqEnd);
  void AddMultiItemToArray(MapItemArray *itemArray, int baseInd,
    float stageX, float stageY, int numXholes, int numYholes, float boxXcen, float boxYcen,
    IntVec &ixSkip, IntVec &iySkip, int groupID, int numInBox, int &numAdded);
};
