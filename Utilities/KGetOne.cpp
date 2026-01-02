// KGetOne.cpp:           Has routines for getting one integer, float, or string
//                          using AskOneDlg
//
// Copyright (C) 2003-2026 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde

#include "stdafx.h"
#include "..\SerialEM.h"
#include "AskOneDlg.h"
#include "KGetOne.h"
#include "..\RadioChoiceDlg.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

// Gets an integer with no info line; return true for OK
BOOL KGetOneInt(CString inText, int &iVal)
{
  return KGetOneInt("", inText, iVal);
}

// Gets an integer with an info line; return true for OK
BOOL KGetOneInt(CString infoText, CString inText, int &iVal)
{
  CAskOneDlg askDlg;
  askDlg.m_sTextString = inText;
  askDlg.m_strInfoLine = infoText;
  askDlg.m_sEditString.Format("%d", iVal);
  if (askDlg.DoModal() == IDOK) {
    if (!askDlg.m_sEditString.IsEmpty()) {
      iVal = atoi((LPCTSTR)askDlg.m_sEditString);
      return true;
    }
  }
  return false;
}

// Gets a float with no info line; return true for OK
BOOL KGetOneFloat(CString inText, float &fVal, int inDecimals)
{
  return KGetOneFloat("", inText, fVal, inDecimals);
}

// Gets a float with an info line; return true for OK
BOOL KGetOneFloat(CString infoText, CString inText, float &fVal, int inDecimals)
{
  char format[6];
  CAskOneDlg askDlg;
  askDlg.m_sTextString = inText;
  askDlg.m_strInfoLine = infoText;
  sprintf(format, "%%.%df", inDecimals);
  askDlg.m_sEditString.Format(format, fVal);
  if (askDlg.DoModal() == IDOK) {
    if (!askDlg.m_sEditString.IsEmpty()) {
      fVal = (float)atof((LPCTSTR)askDlg.m_sEditString);
      return true;
    }
  }
  return false;
}

// Gets a string with no info line; return true for OK
BOOL KGetOneString(CString inText, CString &inEditString, int extraWidth,
                   CString browserTitle)
{
  return KGetOneString("", inText, inEditString, extraWidth, browserTitle);
}

// Gets a string with an info line; return true for OK
BOOL KGetOneString(CString infoText, CString inText, CString &inEditString,
                   int extraWidth, CString browserTitle)
{
  CAskOneDlg askDlg;
  askDlg.m_sTextString = inText;
  askDlg.m_strInfoLine = infoText;
  askDlg.m_sEditString = inEditString;
  askDlg.m_iExtraWidth = extraWidth;  // Optional argument, default 100
  askDlg.m_strBrowserTitle = browserTitle;
  if (askDlg.DoModal() == IDOK) {
    inEditString = askDlg.m_sEditString;
    return true;
  }
  return false;
}

// Presents two or three radio buttons, returns true for OK
BOOL KGetOneChoice(CString infoText1, CString infoText2, int &iVal, CString choice1,
  CString choice2, CString choice3)
{
  CRadioChoiceDlg dlg;
  dlg.mInfoLine1 = infoText1;
  dlg.mInfoLine2 = infoText2;
  dlg.mChoiceOne = choice1;
  dlg.mChoiceTwo = choice2;
  dlg.mChoiceThree = choice3;
  dlg.m_iChoice = iVal;
  if (dlg.DoModal() == IDOK) {
    iVal = dlg.m_iChoice;
    return true;
  }
  return false;
  return 0;
}
