// KGetOne.cpp:           Has routines for getting one integer, float, or string
//                          using AskOneDlg
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde

#include "stdafx.h"
#include "..\SerialEM.h"
#include "AskOneDlg.h"
#include "KGetOne.h"

BOOL KGetOneInt(CString inText, int &iVal)
{
  return KGetOneInt("", inText, iVal);
}

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

BOOL KGetOneFloat(CString inText, float &fVal, int inDecimals)
{
  return KGetOneFloat("", inText, fVal, inDecimals);
}

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

BOOL KGetOneString(CString inText, CString &inEditString, int extraWidth, 
                   CString browserTitle)
{
  return KGetOneString("", inText, inEditString, extraWidth, browserTitle);
}

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
