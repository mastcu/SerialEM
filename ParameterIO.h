// ParameterIO.h: interface for the CParameterIO class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PARAMETERIO_H__A83A6CC0_40E1_4BDF_8FEF_53898673E513__INCLUDED_)
#define AFX_PARAMETERIO_H__A83A6CC0_40E1_4BDF_8FEF_53898673E513__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TiltSeriesParam.h"

struct NavAcqAction;

class DLL_IM_EX CParameterIO
{
public:
	int StringToEntryList(int type, CString str, int &numVals, 
    int *iVals, double *dVals, int maxVals, bool splitCommas = false, short *sVals = NULL);
	CString EntryListToString(int type, int precision, int numVals, int *iVals, double *dVals,
    short *sVals = NULL);
	void WriteShortTermCal(CString strFileName);
	int ReadShortTermCal(CString strFileName, BOOL ignoreCals);
	void WritePlacement(const char *string, int open, WINDOWPLACEMENT *place);
  int ParseString(CString strLine, CString *strItems, int maxItems, bool useQuotes = false,
    int allowComment = 0);
  void WriteCalibration(CString strFile);
  int ReadCalibration(CString strFileName);
  int ReadProperties(CString strFileName);
  void SetDefaultCameraControls(int which, ControlSet *cs, int cameraSizeX, int cameraSizeY);
  BOOL ReadSettings(CString strFileName, bool readingSys = false);
  void WriteFloat(CString format, float fVal);
  void WriteDouble(CString format, double dVal);
  void WriteInt(CString format, int iVal);
  void WriteString(CString format, CString strValue);
  void WriteSettings(CString strFileName);
  void FindToken(CString &strCopy, CString &strItem, bool useQuotes = false, bool allowComment = false);
  int ReadAndParse(CStdioFile *file, CString &strLine, CString *strItems, int maxItems, 
    bool useQuotes = false);
  int ReadAndParse(CString &strLine, CString *strItems, int maxItems, bool useQuotes = false);
  void CheckForSpecialChars(CString &strLine);
  CString *GetDupMessage(void) {return &mDupMessage;};
  CParameterIO();
  virtual ~CParameterIO();

private:
  ControlSet * mConSets;
  CString * mModeNames;
  CSerialEMApp * mWinApp;
  MagTable *mMagTab;
  CStdioFile *mFile;
  int mNumLDSets;
  CameraParameters *mCamParam;
  EMbufferManager *mBufferManager;
  CShiftManager *mShiftManager;
  CArray<FocusTable, FocusTable> *mFocTab;
  LowDoseParams *mLowDoseParams;
  TiltSeriesParam *mTSParam;
  FilterParams *mFilterParam;
  CString mDupMessage;
  int mMaxReadInMacros;
public:
  void StripItems(CString strLine, int numItems, CString & strCopy, bool keepIndent = false, 
    int allowComment = 0);
  int ReadSuperParse(CString &strLine, CString *strItems, BOOL *itemEmpty, int *itemInt,
    double *itemDbl, float *itemFlt, int maxItems);
  int ReadSuperParse(CString &strLine, CString *strItems, BOOL *itemEmpty, int *itemInt,
    double *itemDbl, int maxItems);
  int ReadFlybackTimes(CString & filename);
  void WriteFlybackTimes(CString & filename);
  void InitializeControlSet(ControlSet * cs, int sizeX, int sizeY);
  void ReadBadColumns(CString *strItems, UShortVec &badColumnStart, ShortVec &badColumnWidth);
  void ReadPartialBad(CString *strItems, int *itemInt, UShortVec &partialBadCol, ShortVec &partialBadWidth, 
    UShortVec &partialBadStartY, UShortVec &partialBadEndY, const char *colText, 
    CString &strLine);
  int ReadOneMacro(int iset, CString & strLine, CString * strItems, int maxMacros);
  void WriteAllMacros(int numWrite);
  int ReadMacrosFromFile(CString &filename, const CString &curSettings, int maxMacros, bool printMess = false);
  void WriteMacrosToFile(CString filename, int maxMacros);
  int ReadNavAcqParams(NavAcqParams *navParams, NavAcqAction *mAcqActions, int *actOrder, CString &unrecognized);
  void WriteNavAcqParams(int which, NavAcqParams *navParams, NavAcqAction *mAcqActions, int *actOrder, bool skipNum);
  int ReadAcqParamsFromFile(NavAcqParams *navParams, NavAcqAction *mAcqActions, int *actOrder, CString &filename);
  void WriteAcqParamsToFile(NavAcqParams *navParams, NavAcqAction *mAcqActions, int *actOrder, CString &filename);
  void ReadDisableOrHideFile(CString &filename, std::set<int>  *IDsToHide,
    std::set<int>  *lineHideIDs, std::set<int>  *IDsToDisable, StringSet *stringHides);
  void ReportSpecialOptions(void);
  void PrintAnOption(bool &anyDone, const char *mess);
  void OutputVector(const char * key, int size, ShortVec * shorts, FloatVec* floats);
  void StoreFloatsPerBinning(CString * strItems, const char * descrip, int iset, CString & strFileName, float * values);
  int MacroSetProperty(CString name, double value);
  void UserSetProperty(void);
  int MacroSetCamProperty(CameraParameters *camP, CString &name, double value);
  int MacroSetSetting(CString name, double value);
  int MacroGetSetting(CString name, double &value);
  int MacroGetProperty(CString name, double &value);
  int MacroGetCamProperty(CameraParameters *camP, CString &name, double &value);
  void WriteIndexedInts(const char *keyword, int *values, int numVal);
  void WriteIndexedFloats(const char *keyword, float *values, int numVal);
  int CheckForByteOrderMark(CString & item0, const char * tag, CString & filename, const char *descrip);
};

#endif // !defined(AFX_PARAMETERIO_H__A83A6CC0_40E1_4BDF_8FEF_53898673E513__INCLUDED_)
