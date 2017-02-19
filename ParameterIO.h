// ParameterIO.h: interface for the CParameterIO class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PARAMETERIO_H__A83A6CC0_40E1_4BDF_8FEF_53898673E513__INCLUDED_)
#define AFX_PARAMETERIO_H__A83A6CC0_40E1_4BDF_8FEF_53898673E513__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TiltSeriesParam.h"

class CParameterIO  
{
public:
	int StringToEntryList(int type, CString str, int &numVals, 
    int *iVals, double *dVals, int maxVals);
	CString EntryListToString(int type, int precision, int numVals, int *iVals, double *dVals);
	void WriteShortTermCal(CString strFileName);
	int ReadShortTermCal(CString strFileName);
	void WritePlacement(char *string, int open, WINDOWPLACEMENT *place);
  int ParseString(CString strLine, CString *strItems, int maxItems);
  void WriteCalibration(CString strFile);
  int ReadCalibration(CString strFileName);
  int ReadProperties(CString strFileName);
  void SetDefaultCameraControls(int which, ControlSet *cs, int cameraSizeX, int cameraSizeY);
  BOOL ReadSettings(CString strFileName);
  void WriteFloat(CString format, float fVal);
  void WriteDouble(CString format, double dVal);
  void WriteInt(CString format, int iVal);
  void WriteString(CString format, CString strValue);
  void WriteSettings(CString strFileName);
  void FindToken(CString &strCopy, CString &strItem);
  int ReadAndParse(CStdioFile *file, CString &strLine, CString *strItems, int maxItems);
  int ReadAndParse(CString &strLine, CString *strItems, int maxItems);
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
  CArray<FocusTable, FocusTable> *mFocTab;
  LowDoseParams *mLowDoseParams;
  TiltSeriesParam *mTSParam;
  FilterParams *mFilterParam;
  CString mDupMessage;
public:
  void StripItems(CString strLine, int numItems, CString & strCopy);
  int ReadSuperParse(CString & strLine, CString * strItems, BOOL * itemEmpty, int * itemInt, double * itemDbl, int maxItems);
  int ReadFlybackTimes(CString & filename);
  void WriteFlybackTimes(CString & filename);
  void InitializeControlSet(ControlSet * cs, int sizeX, int sizeY);
  void ReadBadColumns(CString *strItems, UShortVec &badColumnStart, ShortVec &badColumnWidth);
  void ReadPartialBad(CString *strItems, int *itemInt, UShortVec &partialBadCol, ShortVec &partialBadWidth, 
    UShortVec &partialBadStartY, UShortVec &partialBadEndY, const char *colText, 
    CString &strLine);
  int ReadOneMacro(int iset, CString & strLine, CString * strItems);
  void WriteAllMacros(void);
  void ReadMacrosFromFile(CString filename);
  void WriteMacrosToFile(CString filename);
  void ReportSpecialOptions(void);
  void OutputVector(const char * key, int size, ShortVec * shorts, FloatVec* floats);
  void StoreFloatsPerBinning(CString * strItems, const char * descrip, int iset, CString & strFileName, float * values);
  int MacroSetProperty(CString name, double value);
  void UserSetProperty(void);
};

#endif // !defined(AFX_PARAMETERIO_H__A83A6CC0_40E1_4BDF_8FEF_53898673E513__INCLUDED_)
