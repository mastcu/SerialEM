// MyButton.cpp : A button that can
// 1) notify of its selected state when it is drawn, thus allowing responses to press and
// release
// 2) Notify of right mouse up event (right click)
// 3) Draw a background color when a flag is set
// To use in place of a regular button, make a dialog member variable for the button as
// usual, and change CButton * to CMyButton * and include MyButton.h in dialog .h file
// Set the flag m_bNotifyOnDraws true to get notify events from every draw, not just from
// right-clicks
// To receive the notify events you need in the .h file:
//   afx_msg void OnXXXDraw(NMHDR *pNotifyStruct, LRESULT *result);
// and in the .cpp message map:
//   ON_NOTIFY(NM_LDOWN, IDC_BUT_XXX, OnXXXDraw)
// and
//   void CYYYDlg::OnXXXDraw(NMHDR *pNotifyStruct, LRESULT *result)
// the code will be NM_LDOWN regardless.
// m_bRightWasClicked is set after a right click and m_bSelected is true if the button is
// down
//

#include "stdafx.h"
#include "SerialEM.h"
#include "MyButton.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif


// CMyButton

IMPLEMENT_DYNAMIC(CMyButton, CButton)

CMyButton::CMyButton()
{
  m_bShowSpecial = false;
  mSpecialColor = RGB(128, 255, 128);
  m_bRightWasClicked = false;
  m_bNotifyOnDraws = false;
}

CMyButton::~CMyButton()
{
}


BEGIN_MESSAGE_MAP(CMyButton, CButton)
  ON_WM_RBUTTONUP()
END_MESSAGE_MAP()



// CMyButton message handlers


/*
 * This was based on article 480 at codeproject.org by Chris Maunder
 * Create your own controls - the art of subclassing
  https://www.codeproject.com/Articles/480/Create-your-own-controls-the-art-of-subclassing
 */

// Custom draw replicates the needed actions but can add a background color
// sets flag about button selected, sends WM_NOTIFY message
void CMyButton::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
  CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);
  CRect rect = lpDrawItemStruct->rcItem;
  UINT state = lpDrawItemStruct->itemState;
  NMHDR hdr;

  CString strText;
  GetWindowText(strText);

  // draw the control edges (DrawFrameControl is handy!)
  m_bSelected = (state & ODS_SELECTED) != 0;
  if (m_bSelected)
    pDC->DrawFrameControl(rect, DFC_BUTTON, DFCS_BUTTONPUSH | DFCS_PUSHED);
  else
    pDC->DrawFrameControl(rect, DFC_BUTTON, DFCS_BUTTONPUSH);

  // Deflate the drawing rect by the size of the button's edges
  rect.DeflateRect(CSize(GetSystemMetrics(SM_CXEDGE), GetSystemMetrics(SM_CYEDGE)));

  // Fill the interior color if necessary
  if (m_bShowSpecial)
    pDC->FillSolidRect(rect, mSpecialColor);

                                                // Draw the text
  if (!strText.IsEmpty())
  {
    CSize Extent = pDC->GetTextExtent(strText);
    CPoint pt(rect.CenterPoint().x - Extent.cx / 2,
      rect.CenterPoint().y - Extent.cy / 2);

    if (m_bSelected)
      pt.Offset(1, 1);

    int nMode = pDC->SetBkMode(TRANSPARENT);

    if (state & ODS_DISABLED)
      pDC->DrawState(pt, Extent, strText, DSS_DISABLED, TRUE, 0, (HBRUSH)NULL);
    else
      pDC->TextOut(pt.x, pt.y, strText);

    pDC->SetBkMode(nMode);
  }

  // Send the message.  Sending NM_LDOWN because nothing else seems to get through!
  if (m_bNotifyOnDraws) {
    hdr.code = NM_LDOWN;
    hdr.hwndFrom = this->GetSafeHwnd();
    hdr.idFrom = GetDlgCtrlID();
    this->GetParent()->SendMessage(WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);
  }
}

// This is needed to activate owner draw without selecting in GUI properties
void CMyButton::PreSubclassWindow()
{
  CButton::PreSubclassWindow();

  ModifyStyle(0, BS_OWNERDRAW);	// make the button owner drawn
}

// On right button up, this sets a flag that there was a right-click, which should be
// cleared by the receiver if it is tracking right clicks and not presses, and it sends
// a WM_NOTIFY with the SAME code as for a draw, because that is the only code that works!
void CMyButton::OnRButtonUp(UINT nFlags, CPoint point)
{
  NMHDR hdr;
  m_bRightWasClicked = true;
  hdr.code = NM_LDOWN;
  hdr.hwndFrom = this->GetSafeHwnd();
  hdr.idFrom = GetDlgCtrlID();
  this->GetParent()->SendMessage(WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);
}
