#ifndef SEMUTILITIES_H
#define SEMUTILITIES_H

#define RELEASE_RETURN_ON_ERR(a, b) if (a) { AdocReleaseMutex() ; return b;}

struct MontParam;
struct CameraParameters;
class KStoreMRC;
const char *SEMCCDErrorMessage(int code);
int MontageConSetNum(MontParam *param, bool trueSet, int lowDose = -1);
int MontageLDAreaIndex(MontParam *param);
void SEMUtilInitialize();
bool UtilOKtoAllocate(int numBytes);
int UtilThreadBusy(CWinThread **threadpp, DWORD *exitPtr = NULL);
void UtilThreadCleanup(CWinThread **threadpp);
KStoreMRC *UtilOpenOldMRCFile(CString filename);
int UtilOpenFileReadImage(CString filename, CString descrip);
int UtilSaveSingleImage(CString filename, CString descrip, bool useMdoc);
void UtilRemoveFile(CString filename);
int UtilRenameFile(CString fromName, CString toName, const char *message = NULL);
void UtilSplitExtension(CString filename, CString &root, CString &ext);
void UtilSplitPath(CString fullPath, CString &directory, CString &filename);
void UtilAppendWithSeparator(CString &filename, CString toAdd, const char* sep);
void UtilBalancedGroupLimits(int numTotal, int numGroups, int groupInd, int &start, 
                         int &end);
int FindIndexForMagValue(int magval, int camera = -1, int magInd = -1);
int DLL_IM_EX MagOrEFTEMmag(BOOL GIFmode, int index, BOOL STEMcam = false);
int MagForCamera(CameraParameters *camParam, int index);
int MagForCamera(int camera, int index);
bool UtilInvertedMagRangeLimits(BOOL EFTEM, int &lowInd, int &highInd);
bool UtilMagInInvertedRange(int magInd, BOOL EFTEM);
void ConstrainWindowPlacement(int *left, int *top, int *right, int *bottom);
void ConstrainWindowPlacement(WINDOWPLACEMENT *place);
void UtilGpuMemoryNeeds(float fullPadSize, float sumPadSize, float alignPadSize,
                    int numAllVsAll, int nzAlign, int refineAtEnd, int groupSize,
                    float &needForGpuSum, float &needForGpuAli);
float UtilTotalMemoryNeeds(float fullPadSize, float sumPadSize, float alignPadSize,
                       int numAllVsAll, int nzAlign, int refineAtEnd, int numBinTests,
                       int numFiltTests, int hybridShifts, int groupSize, int doSpline,
                       int gpuFlags, int deferSum, int testMode, int startAssess,
                       bool &sumInOnePass);
void UtilGetPadSizesBytes(int nx, int ny, float fullTaperFrac, int sumBin, int alignBin,
                      float &fullPadSize, float &sumPadSize, float &alignPadSize);
int AdocGetMutexSetCurrent(int index);
void AdocReleaseMutex();
BOOL AdocAcquireMutex();
int CamLDParamIndex(int camera = -1);
int CamLDParamIndex(CameraParameters *camParam);
bool NewSpinnerValue(NMHDR *pNMHDR, LRESULT *pResult, int oldVal, int lowerLim,
  int upperLim, int &newVal);
bool NewSpinnerValue(NMHDR *pNMHDR, LRESULT *pResult, int lowerLim, int upperLim,
  int &newVal);
void SetDropDownHeight(CComboBox* pMyComboBox, int itemsToShow);
bool UtilCamRadiosNeedSmallFont(CButton *radio);
double UtilGoodAngle(double angle);
int UtilFindValidFrameAliParams(int readMode, int whereAlign, int curIndex, int &newIndex,
  CString *message);
#endif
