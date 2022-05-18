#ifndef SEMUTILITIES_H
#define SEMUTILITIES_H
struct FrameAliParams;
#include <string>
#include <vector>

#define MAX_DROPDOWN_TO_SHOW 40

#define RELEASE_RETURN_ON_ERR(a, b) if (a) { AdocReleaseMutex() ; return b;}

struct MontParam;
struct CameraParameters;
class KImageStore;
const char *SEMCCDErrorMessage(int code);
int MontageConSetNum(MontParam *param, bool trueSet, int lowDose = -1);
int MontageLDAreaIndex(MontParam *param);
bool UseMultishotForMontage(MontParam *param, int lowDose = -1);
void SEMUtilInitialize();
bool UtilOKtoAllocate(int numBytes);
int UtilThreadBusy(CWinThread **threadpp, DWORD *exitPtr = NULL);
void UtilThreadCleanup(CWinThread **threadpp);
KImageStore *UtilOpenOldMRCFile(CString filename);
int UtilOpenFileReadImage(CString filename, CString descrip);
int UtilSaveSingleImage(CString filename, CString descrip, bool useMdoc);
int UtilCheckDiskFreeSpace(float needed, const char *operation);
void UtilRemoveFile(CString filename);
int UtilRenameFile(CString fromName, CString toName, CString *message = NULL, bool returnMess = false);
void UtilSplitExtension(CString filename, CString &root, CString &ext);
void UtilSplitPath(CString fullPath, CString &directory, CString &filename);
int UtilRelativePath(std::string fromDir, std::string toDir, std::string &relPath);
int UtilStandardizePath(std::string &dir);
int UtilRelativePath(CString fromDir, CString toDir, CString &relPath);
int UtilStandardizePath(CString &dir);
int UtilRecursiveMakeDir(CString dir, CString &mess);
void UtilAppendWithSeparator(CString &filename, CString toAdd, const char* sep);
char UtilCheckIllegalChars(CString &filename, int slashOrDriveOK, CString descrip);
void DLL_IM_EX UtilTrimTrailingZeros(CString &str);
int CreateFrameDirIfNeeded(CString &directory, CString *errStr, char debug);
void UtilBalancedGroupLimits(int numTotal, int numGroups, int groupInd, int &start, 
                         int &end);
void UtilInterpolatePeak(std::vector<float> &vecPos, std::vector<float> &vecPeak,
  float step, float peakmax, float &posBest);
int FindIndexForMagValue(int magval, int camera = -1, int magInd = -1);
int DLL_IM_EX MagOrEFTEMmag(BOOL GIFmode, int index, BOOL STEMcam = false);
int MagForCamera(CameraParameters *camParam, int index);
int MagForCamera(int camera, int index);
int BinDivisorI(CameraParameters *camParam);
float BinDivisorF(CameraParameters *camParam);
bool CamHasDoubledBinnings(CameraParameters *camParam);
bool UtilInvertedMagRangeLimits(BOOL EFTEM, int &lowInd, int &highInd);
bool UtilMagInInvertedRange(int magInd, BOOL EFTEM);
void ConstrainWindowPlacement(int *left, int *top, int *right, int *bottom, bool scale);
void ConstrainWindowPlacement(WINDOWPLACEMENT *place, bool scale);
float UtilEvaluateGpuCapability(int nx, int ny, int dataSize, bool gainNorm, bool defects,
  int sumBinning,  FrameAliParams &faParam, int numAllVsAll, int numAliFrames, 
  int refineIter, int groupSize, int numFilt, int doSpline, double gpuMemory, 
  double maxMemory, int &gpuFlags, int &deferGpuSum, bool &gettingFRC);
int UtilGetFrameAlignBinning(FrameAliParams & param, int frameSizeX, int frameSizeY);
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
void LoadMacrosIntoDropDown(CComboBox &combo, bool useLong, bool addNone);
void UtilModifyMenuItem(const char *popupName, UINT itemID, const char * newText);
void UtilGetMenuString(CMenu *menu, int position, CString &name, UINT nFlags);
bool UtilCamRadiosNeedSmallFont(CButton *radio);
double UtilGoodAngle(double angle);
int UtilFindValidFrameAliParams(CameraParameters *camParam, int readMode, bool takeK3Binned, 
  int whereAlign, int curIndex, int &newIndex, CString *message);
int UtilWriteTextFile(CString fileName, CString text);
CString FormattedNumber(double value, const char *suffix, int minDec, int maxDec, float switchVal);
BOOL SleepMsg(DWORD dwTime_ms);
#endif
