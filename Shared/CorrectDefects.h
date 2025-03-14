// Declarations for CorrectDefects.cpp and the CameraDefects structure
//
// This file is the same between IMOD and SEMCCD but SerialEM needs a variation with
// a FloatVec typedef, DLL_IM_EX definition and DLL_IM_EX on CorDefUserToRotFlipCCD
#ifndef CORRECT_DEFECTS_H
#define CORRECT_DEFECTS_H
#include <vector>
#include <string>
typedef std::vector<short int> ShortVec;
typedef std::vector<unsigned short int> UShortVec;
typedef std::vector<int> IntVec;

#ifdef _SERIALEM
typedef std::vector<float> FloatVec;
#ifdef _WIN32
#ifndef DLL_IM_EX
#define DLL_IM_EX  _declspec(dllexport)
#endif
#endif
#endif

typedef struct  ImodImageFileStruct ImodImageFile;

#define MAX_AVG_SUPER_RES 4
#define MAP_COL_AVG_SUPER 254
#define MAP_ROW_AVG_SUPER 255

struct CameraDefects
{
  int wasScaled;           // flag for whether data have been scaled: 1 if up, -1 if down
  int rotationFlip;        // Rotation - flip value of these coordinates relative to CCD
  int K2Type;              // Flag that camera is K2
  int FalconType;          // Flag that camera is Falcon
  int usableTop;           // Usable area, defined by first and last good rows and columns
  int usableLeft;
  int usableBottom;
  int usableRight;
  int numAvgSuperRes;      // # of adjacent physical pixels to average super-res pixels in
  UShortVec badColumnStart;  // First column of each entry
  ShortVec badColumnWidth;  // Number of adjacent bad columns
  UShortVec partialBadCol;   // the partial starting column
  ShortVec partialBadWidth;   // the number of columns
  UShortVec partialBadStartY;  // Start and end in Y
  UShortVec partialBadEndY;
  UShortVec badRowStart;  // First row of each entry
  ShortVec badRowHeight;  // Number of adjacent bad rows
  UShortVec partialBadRow;   // the partial starting row
  ShortVec partialBadHeight;   // the number of rows
  UShortVec partialBadStartX;  // Start and end in X
  UShortVec partialBadEndX;
  UShortVec badPixelX;    // Bad pixel X and Y coordinates
  UShortVec badPixelY;
  std::vector<char>pixUseMean;  // Flag for pixels to fill with mean, touch other defects
};

void CorDefCorrectDefects(CameraDefects *param, void *array, int type, int binning,
                        int top, int left, int bottom, int right);
float CorDefSurroundingMean(void *frame, int type, int nx, int ny, float truncLimit,
                            int ix, int iy);
void CorDefScaleDefectsForK2(CameraDefects *param, bool scaleDown);
void CorDefScaleDefectsForFalcon(CameraDefects *param, int factor);
void CorDefFlipDefectsInY(CameraDefects *param, int camSizeX, int camSizeY, int wasScaled);
void CorDefMergeDefectLists(CameraDefects &defects, unsigned short *xyPairs, 
                            int numPoints, int camSizeX, int camSizeY, int rotationFlip);
void CorDefMirrorCoords(int size, int binning, int &start, int &end);
#ifdef _SERIALEM
void DLL_IM_EX CorDefUserToRotFlipCCD(int operation, int binning, int &camSizeX, int &camSizeY, int &imSizeX,
                            int &imSizeY, int &top, int &left, int &bottom, int &right);
#else
void CorDefUserToRotFlipCCD(int operation, int binning, int &camSizeX, int &camSizeY, int &imSizeX,
  int &imSizeY, int &top, int &left, int &bottom, int &right);
#endif
void CorDefRotFlipCCDtoUser(int operation, int binning, int &camSizeX, int &camSizeY, int &imSizeX,
                            int &imSizeY, int &top, int &left, int &bottom, int &right);
void CorDefRotateCoordsCW(int binning, int &camSizeX, int &camSizeY, int &imSizeX,
                          int &imSizeY, int &top, int &left, int &bottom, int &right);
void CorDefRotateCoordsCCW(int binning, int &camSizeX, int &camSizeY, int &imSizeX,
                           int &imSizeY, int &top, int &left, int &bottom, int &right);
void CorDefRotFlipCCDcoord(int operation, int camSizeX, int camSizeY, int &xx, int &yy);
void CorDefDefectsToString(CameraDefects &defects, std::string &strng, int camSizeX,
                           int camSizeY);
void CorDefAddBadColumn(int col, UShortVec &badColumnStart, ShortVec &badColumnWidth);
void CorDefAddPartialBadCol(int *values,  UShortVec &partialBadCol, 
                                  ShortVec &partialBadWidth, UShortVec &partialBadStartY, 
                                  UShortVec &partialBadEndY);
void CorDefRotateFlipDefects(CameraDefects &defects, int rotationFlip, int camSizeX,
                             int camSizeY);
void CorDefFindTouchingPixels(CameraDefects &defects, int camSizeX, int camSizeY, int wasScaled);
int CorDefFindDriftCorrEdges(void *array, int type, int nx, int ny, int analyzeLen,
                             int maxWidth, float critMADNs, int &xLow, int &xHigh,
                             int &yLow, int &yHigh);
void CorDefSampleMeanSD(void *array, int type, int nxdim, int nx, int ny, int ixStart,
                        int iyStart, int nxUse, int nyUse, float *mean, float *sd);
void CorDefSampleMeanSD(void *array, int type, int nxdim, int nx, int ny, float *mean, 
                        float *sd);
void CorDefSampleMeanSD(void *array, int type, int nx, int ny, float *mean, float *sd);
int CorDefParseDefects(const char *strng, int fromString, CameraDefects &defects, 
                        int &camSizeX, int &camSizeY);
int CorDefParseFeiXml(const char *strng, CameraDefects &defects, int pad);
int CorDefProcessFeiDefects(ImodImageFile *iiFile, CameraDefects &defects, int nx, int ny,
                            bool flipY, int superFac, int feiDefPad,
                            const char *dumpDefectName, char *messBuf, int  bufLen);
int CorDefSetupToCorrect(int nxFull, int nyFull, CameraDefects &defects, int &camSizeX,
                       int &camSizeY, int scaleDefects, float setBinning,
                       int &useBinning, const char *binOpt);
int CorDefFillDefectArray(CameraDefects *param, int camSizeX, int camSizeY,
                          unsigned char *array, int nx, int ny, bool doFalconPad);
void CorDefExpandGainReference(float *refIn, int nxIn, int nyIn, int factor,
                               float *refOut);
int CorDefReadSuperGain(const char *filename, int superFac, 
                        std::vector<std::vector<float> > &biases, int &numInX,
                        int &xStart, int &xSpacing, int &numInY, int &yStart,
                        int &ySpacing);
void CorDefRefineSuperResRef(float *ref, int nx, int ny, int superFac, 
                             std::vector<std::vector<float> > &biases, int numInX,
                             int xStart, int xSpacing, int numInY, int yStart, 
                             int ySpacing);
#endif
