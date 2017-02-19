#ifndef KGETONE_H
#define KGETONE_H
BOOL KGetOneInt(CString inText, int &iVal);
BOOL KGetOneFloat(CString inText, float &fVal, int inDecimals);
BOOL KGetOneInt(CString infoText, CString inText, int &iVal);
BOOL KGetOneFloat(CString infoText, CString inText, float &fVal, int inDecimals);
BOOL KGetOneString(CString inText, CString &inEditString, int extraWidth = 100,
                   CString browserTitle = "");
BOOL KGetOneString(CString infoText, CString inText, CString &inEditString, 
                   int extraWidth = 100, CString browserTitle = "");
#endif