////////////////////////////////////////////////////////////////
// PixieLib(TM) Copyright 1997-2005 Paul DiLascia
// If this code works, it was written by Paul DiLascia.
// If not, I don't know who wrote it.
// Compiles with Visual Studio.NET 7.1 or greater. Set tabsize=3.
// 
// NOTE: PixieLib is NOT freeware! 
// If you didn't pay for your copy, you're violating my copyright!
//
#include "stdafx.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

enum {
	TIMER_SHOW = 1,
	TIMER_CANCEL,
};

IMPLEMENT_DYNAMIC(CPopupText,CWnd)
BEGIN_MESSAGE_MAP(CPopupText,CWnd)
	ON_WM_PAINT()
	ON_MESSAGE(WM_SETTEXT, OnSetText)
	ON_WM_TIMER()
END_MESSAGE_MAP()

CPopupText::CPopupText()
{
	CNonClientMetrics ncm;
	m_font.CreateFontIndirect(&ncm.lfMenuFont);
	m_szMargins = CSize(4,4);
	m_clrBG   = GetSysColor(COLOR_INFOBK);		// tooltip background color
	m_clrText = GetSysColor(COLOR_INFOTEXT);	// tooltip text color
	m_dfltDrawFlags = DT_SINGLELINE|DT_VCENTER|DT_CENTER;
}

CPopupText::~CPopupText()
{
}

//////////////////
// Create window. pt is upper left corner
//
int CPopupText::Create(CPoint pt, CWnd* pParentWnd, UINT nID)
{
	return CreateEx(0,
		NULL,
		NULL,
		WS_POPUP|WS_VISIBLE,
		CRect(pt,CSize(0,0)),
		pParentWnd,
		nID);
}

//////////////////
// Text changed: resize window to fit
//
LRESULT CPopupText::OnSetText(WPARAM /* wp */, LPARAM lp)
{
	CRect rc;
	GetWindowRect(&rc);
	int h = rc.Height();
	CClientDC dc(this);
	// calculate text rectangle
	DrawText(dc, CString((LPCTSTR)lp), rc, DT_CALCRECT); // calculate text rect
	rc.InflateRect(m_szMargins);								  // add margins
	SetWindowPos(NULL, 0, 0, rc.Width(), h,
		SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);
	return Default();
}

//////////////////
// Paint text using system tooltip colors and font
//
void CPopupText::DrawText(CDC& dc, LPCTSTR lpText, CRect& rc, UINT flags)
{
	CBrush b(m_clrBG);
	dc.FillRect(&rc, &b);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(m_clrText); // tooltip text color
	CFont* pOldFont = dc.SelectObject(&m_font);
	dc.DrawText(lpText, &rc, flags);
	dc.SelectObject(pOldFont);
}

//////////////////
// Paint text
//
void CPopupText::OnPaint()
{
	CRect rc;
	GetClientRect(&rc);
	CString s;
	GetWindowText(s);
	CPaintDC dc(this);
	DrawText(dc, s, rc, m_dfltDrawFlags);
}

//////////////////
// Register class if needed
//
BOOL CPopupText::PreCreateWindow(CREATESTRUCT& cs) 
{
	static CString sClassName;
	if (sClassName.IsEmpty())
		sClassName = AfxRegisterWndClass(0);
	cs.lpszClass = sClassName;
	cs.style = WS_POPUP|WS_BORDER;
	cs.dwExStyle |= WS_EX_TOOLWINDOW;
	return CWnd::PreCreateWindow(cs);
}

//////////////////
// CPopupText is intended to be used on the stack or in objects--not
// heap--just like a control, so don't auto-delete.
//
void CPopupText::PostNcDestroy()
{
	// don't delete this--return instead of passing to CWnd.
}

//////////////////
// Show window with delay. No delay means show now.
// Can also specify an auto-timeout to cancel tip.
//
void CPopupText::ShowDelayed(UINT msec, UINT msecTimeout)
{
	m_msecTimeout = msecTimeout;
	if (msec==0) {
		OnTimer(1);	// no delay: show it now
		if (msecTimeout>0) {
			SetTimer(TIMER_CANCEL, msecTimeout, NULL); // auto-cancel
		}

	} else {
		SetTimer(TIMER_SHOW, msec, NULL); // delay: set timer
	}
}

//////////////////
// Cancel: kill timer and hide window
//
void CPopupText::Cancel()
{
	if (m_hWnd) {
		KillTimer(TIMER_SHOW);
		KillTimer(TIMER_CANCEL);
		ShowWindow(SW_HIDE);
	}
}

//////////////////
// Timer popped: display myself and kill timer
//
void CPopupText::OnTimer(UINT_PTR nTimer)
{
	if (nTimer==TIMER_SHOW) {
		ShowWindow(SW_SHOWNA);
		Invalidate();
		UpdateWindow();
		KillTimer(TIMER_SHOW);
		if (m_msecTimeout>0) {
			SetTimer(TIMER_CANCEL, m_msecTimeout, NULL); // auto-cancel
		}
	} else if (nTimer==TIMER_CANCEL) {
		Cancel();
	} else {
		ASSERT(FALSE); // should never happen
	}
}
