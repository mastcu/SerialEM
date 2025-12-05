// CXSliderCtrl - colored slider with downward pointing tip
// Based on Customizing the Appearance of CSliderCtrl Using Custom Draw
// https://www.codeproject.com/articles/Customizing-the-Appearance-of-CSliderCtrl-Using-Cu#comments-section
// By  Mike O'Neill
//
// Note that the slider needs to be updated with a new value using SetPos() BEFORE any
// UpdateData(false) that might load values in

#include "stdafx.h"
#include "SerialEM.h"
#include "XSliderCtrl.h"

CXSliderCtrl::CXSliderCtrl()
{
  mIdleCR = RGB(0, 80, 200);
  mHighlightCR = RGB(0, 100, 255);
  mTrimFrac = 0.1f;
}

CXSliderCtrl::~CXSliderCtrl()
{
}


BEGIN_MESSAGE_MAP(CXSliderCtrl, CSliderCtrl)
    //{{AFX_MSG_MAP(CXSliderCtrl)
    // NOTE - the ClassWizard will add and remove mapping macros here.
    ON_NOTIFY_REFLECT( NM_CUSTOMDRAW, OnCustomDraw )
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CXSliderCtrl message handlers

void CXSliderCtrl::SetColors(COLORREF cr, COLORREF crHighlight)
{
  mIdleCR = cr;
  mHighlightCR = crHighlight;
  m_Pen.CreatePen(PS_SOLID, 1, RGB(128, 128, 128)); // dark gray
}

afx_msg void CXSliderCtrl::OnCustomDraw(NMHDR * pNotifyStruct, LRESULT* result)
{
  POINT pts[5];
  int ll, rr, bb, tt, del, xtrim, ytrim;
  NMCUSTOMDRAW nmcd = *(LPNMCUSTOMDRAW)pNotifyStruct;
  //PrintfToLog("%p stage %x item %x", this, nmcd.dwDrawStage, nmcd.dwItemSpec);
  if (nmcd.dwDrawStage == CDDS_PREPAINT) {
    // return CDRF_NOTIFYITEMDRAW so that we will get subsequent 
    // CDDS_ITEMPREPAINT notifications
    *result = CDRF_NOTIFYITEMDRAW;
    return;
  } else if (nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
    if (nmcd.dwItemSpec == TBCD_THUMB) {

      // The brush had to be created here, not as a member variable
      CBrush brush;
      brush.CreateSolidBrush((nmcd.uItemState & CDIS_SELECTED) ? mHighlightCR : mIdleCR);
      CDC* pDC = CDC::FromHandle(nmcd.hdc);
      pDC->SelectObject(&brush);
      pDC->SelectObject(&m_Pen);
      xtrim = B3DNINT(mTrimFrac * (nmcd.rc.right - nmcd.rc.left));
      ytrim = B3DNINT(mTrimFrac * (nmcd.rc.bottom - nmcd.rc.top));
      ll = nmcd.rc.left + xtrim;
      rr = nmcd.rc.right - xtrim;
      tt = nmcd.rc.top + ytrim;
      bb = nmcd.rc.bottom - ytrim;
      del = B3DNINT((bb - tt) / 3.5);
      pts[0].x = ll;
      pts[0].y = tt;
      pts[1].x = ll;
      pts[1].y = bb - del;
      pts[2].x = (ll + rr) / 2;
      pts[2].y = bb;
      pts[3].x = rr;
      pts[3].y = bb - del;
      pts[4].x = rr;
      pts[4].y = tt;
      pDC->Polygon(pts, 5);
      pDC->Detach();
      *result = CDRF_SKIPDEFAULT;
    }
  }
}

