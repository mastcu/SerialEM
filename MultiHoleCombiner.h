#pragma once

class CMapDrawItem;
class HoleFinder;
class CNavHelper;
class CNavigatorDlg;

enum { COMBINE_ON_IMAGE = 0, COMBINE_IN_POLYGON, COMBINE_IN_GROUP };

struct PositionData
{
  int startX, endX;
  int startY, endY;
  int numAcquires;
};

class CMultiHoleCombiner
{
public:
  CMultiHoleCombiner(void);
  ~CMultiHoleCombiner(void);

  int CombineItems(int boundType);
  bool OKtoUndoCombine(void);
  void UndoCombination(void);
  const char *GetErrorMessage(int error);

private:
  CNavHelper *mHelper;
  CSerialEMApp *mWinApp;
  CNavigatorDlg *mNav;
  HoleFinder *mFindHoles;
  int mNxGrid, mNyGrid, mNumXholes, mNumYholes;
  int **mGrid;
  CArray<CMapDrawItem *, CMapDrawItem *>mSavedItems;
  IntVec mIDsForUndo;
  ScaleMat mSkipXform;
  bool mTransposeSize;

  void TryBoxStartsOnLine(int otherStart, bool doCol,
    CArray<PositionData, PositionData> &fullArray);
  void EvaluateLineOfBoxes(int xStart, int yStart, bool doCol,
    CArray<PositionData, PositionData> &posArray, float &sdOfNums);
  void EvaluateBoxAtPosition(int xStart, int yStart, PositionData &data);
  void EvaluateCrossAtPosition(int xCen, int yCen, PositionData &data);

  void mergeBoxesAndEvaluate(CArray<PositionData, PositionData> &posArray, float &sdOfNums);
  void SetBoxAcquireLimits(int start, int end, int numHoles, int nGrid, int &acqStart, int &acqEnd);
  void AddMultiItemToArray(CArray<CMapDrawItem *, CMapDrawItem *> *itemArray, int baseInd,
    float stageX, float stageY, int numXholes, int numYholes, float boxXcen, float boxYcen,
    IntVec &ixSkip, IntVec &iySkip);
  void ClearSavedItemArray(void);
};