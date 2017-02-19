// ChildFrm.cpp          Standard MFC MDI component, contains an individual
//                         instance of a CSerialEMView window
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde

#include "stdafx.h"
#include "SerialEM.h"

#include "ChildFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CChildFrame

IMPLEMENT_DYNCREATE(CChildFrame, CMDIChildWnd)

BEGIN_MESSAGE_MAP(CChildFrame, CMDIChildWnd)
  //{{AFX_MSG_MAP(CChildFrame)
  ON_WM_CLOSE()
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CChildFrame construction/destruction

CChildFrame::CChildFrame()
{
  SEMBuildTime(__DATE__, __TIME__);
  mStaticFrame = false; 
}

CChildFrame::~CChildFrame()
{
}

BOOL CChildFrame::PreCreateWindow(CREATESTRUCT& cs)
{
  // TODO: Modify the Window class or styles here by modifying
  //  the CREATESTRUCT cs

  // This gave the wrong size ClientRect, which included the status and toolbars
  /* 
  RECT rect;
  if (::GetClientRect(cs.hwndParent, &rect)) {
    cs.y = rect.top;
    cs.x = rect.left + 150;
    cs.cx = rect.right - rect.left - 150;
    cs.cy = rect.bottom - rect.top;
  }
  */

  // If this is the first window, eliminate the title bar
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  if (!winApp->mMainView)
    cs.style &= ~(WS_MAXIMIZEBOX | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);

  // Prevent document from setting the title
  cs.style &= ~FWS_ADDTOTITLE;

  if( !CMDIChildWnd::PreCreateWindow(cs) )
    return FALSE;

  return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CChildFrame diagnostics

#ifdef _DEBUG
void CChildFrame::AssertValid() const
{
  CMDIChildWnd::AssertValid();
}

void CChildFrame::Dump(CDumpContext& dc) const
{
  CMDIChildWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CChildFrame message handlers

void CChildFrame::OnClose() 
{
  // If this is frame for the static window, cancel the close
  if (mStaticFrame)
    return; 

  // Otherwise, need to tell view to destroy its image buffer

  CMDIChildWnd::OnClose();
}
