////////////////////////////////////////////////////////////////
// Extracted from PixieLib and adapted for SerialEM.  Original copyright notice:
//
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

//////////////////
// This is more or less copied from CFrameWnd::GetMessageString, but
// I want a static function that doesn't require a frame window.
//
CString CMenuTipManager::GetResCommandPrompt(UINT nID)
{
	// load appropriate string
	CString s;
	if (s.LoadString(nID)) {
		LPTSTR lpsz = s.GetBuffer(255);
		// first newline terminates prompt
		lpsz = _tcschr(lpsz, '\n');
		if (lpsz != NULL)
			*lpsz = '\0';
		s.ReleaseBuffer();
	}
	return s;
}

//////////////////
// Override CSubclassWnd::WindowProc to hook messages on behalf of
// main window.
//
LRESULT CMenuTipManager::WindowProc(UINT msg, WPARAM wp, LPARAM lp)
{
	if (msg==WM_MENUSELECT) {
		OnMenuSelect(LOWORD(wp), HIWORD(wp), (HMENU)lp);
	} else if (msg==WM_ENTERIDLE) {
		OnEnterIdle(wp, (HWND)lp);
	}
	return CSubclassWnd::WindowProc(msg, wp, lp);
}

//////////////////
// Got WM_MENUSELECT: show tip.
//
void CMenuTipManager::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hMenu)
{
	CPopupText& tip = m_wndTip;

	if (!tip.m_hWnd) {
		tip.Create(CPoint(0,0), CWnd::FromHandle(m_hWnd));
		tip.m_szMargins = CSize(4,0);
	}

	if ((nFlags & 0xFFFF)==0xFFFF) {
		tip.Cancel();	// cancel/hide tip
		m_bMouseSelect = FALSE;
		m_bSticky = FALSE;

	} else if (nFlags & MF_POPUP) {
		tip.Cancel();	// new popup: cancel
		m_bSticky = FALSE;

	} else if (nFlags & MF_SEPARATOR) {
		// separator: hide tip but remember sticky state
		m_bSticky = tip.IsWindowVisible();
		tip.Cancel();

	} else if (nItemID && hMenu) {
		// if tips already displayed, keep displayed
		m_bSticky = tip.IsWindowVisible() || m_bSticky;

		// remember if mouse used to invoke menu
		m_bMouseSelect = (nFlags & MF_MOUSESELECT)!=0;

		// get prompt and display tip (with or without timeout)
		CString prompt = OnGetCommandPrompt(nItemID);
		if (prompt.IsEmpty())
			tip.Cancel(); // no prompt: cancel tip

		else {
			CRect rc = GetMenuTipRect(hMenu, nItemID);
			tip.SetWindowPos(&CWnd::wndTopMost, rc.left, rc.top,
				rc.Width(), rc.Height(), SWP_NOACTIVATE);
			tip.SetWindowText(prompt);
			tip.ShowDelayed(m_bSticky ? 0 : m_iDelay);

      //added for SerialEM 9/9/26: move tip to the left side if it would go off the screen
      //---------------------------------------------------------------------------
      CRect rcMenu;
      CWnd* pWndMenu = GetRunningMenuWnd();
      pWndMenu->GetWindowRect(rcMenu); // whole menu rect

      // Get the monitor where the menu is located (or use primary monitor)
      HMONITOR hMonitor = MonitorFromRect(&rcMenu, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi = { sizeof(MONITORINFO) };
      GetMonitorInfo(hMonitor, &mi);
      CRect rcWorkArea = mi.rcWork; // working area of the monitor
      
      // Check if tip would go off the right edge of the screen
      tip.GetWindowRect(&rc);
      if (rcMenu.right + rc.Width() > rcWorkArea.right) {
        // Place tip to the left of the menu instead
        rc.left = rcMenu.left - rc.Width();
        rc.right = rcMenu.left;
      }
      tip.SetWindowPos(&CWnd::wndTopMost, rc.left, rc.top,
        rc.Width(), rc.Height(), SWP_NOACTIVATE);
      //---------------------------------------------------------------------------

		}
	}
}

//////////////////
// Calculate position of tip: next to menu item.
//
CRect CMenuTipManager::GetMenuTipRect(HMENU hmenu, UINT nID)
{
	CWnd* pWndMenu = GetRunningMenuWnd(); //CWnd::WindowFromPoint(pt);
	ASSERT(pWndMenu);

	CRect rcMenu;
	pWndMenu->GetWindowRect(rcMenu); // whole menu rect

	// add heights of menu items until i reach nID
	int count = ::GetMenuItemCount(hmenu);
	int cy = rcMenu.top + GetSystemMetrics(SM_CYEDGE)+1;
	for (int i=0; i<count; i++) {
		CRect rc;
		::GetMenuItemRect(m_hWnd, hmenu, i, &rc);
		if (::GetMenuItemID(hmenu,i)==nID) {
			// found menu item: adjust rectangle to right and down
			rc += CPoint(rcMenu.right - rc.left, cy - rc.top);

			return rc;
		}
		cy += rc.Height(); // add height
	}
	return CRect(0,0,0,0);
}

//////////////////
// Note that windows are enumerated in top-down Z-order, so the menu
// window should always be the first one found.
//
static BOOL CALLBACK MyEnumProc(HWND hwnd, LPARAM lParam)
{
	TCHAR buf[16];
	GetClassName(hwnd,buf,sizeof(buf));
	if (_tcscmp(buf,_T("#32768"))==0) { // special class name for menus
		*((HWND*)lParam) = hwnd;	 // found it
		return FALSE;
	}
	return TRUE;
}

//////////////////
// Get running menu window.
//
CWnd* CMenuTipManager::GetRunningMenuWnd()
{
	HWND hwnd = NULL;
	EnumWindows(MyEnumProc,(LPARAM)&hwnd);
	return CWnd::FromHandle(hwnd);
}

//////////////////
// Need to handle WM_ENTERIDLE to cancel the tip if the user moved
// the mouse off the popup menu. For main menus, Windows will send a
// WM_MENUSELECT message for the parent menu when this happens, but for
// context menus there's no other way to know the user moved the mouse
// off the menu.
//
void CMenuTipManager::OnEnterIdle(WPARAM nWhy, HWND hwndWho)
{
	if (m_bMouseSelect && nWhy==MSGF_MENU) {
		CPoint pt;
		GetCursorPos(&pt);
		if (hwndWho != ::WindowFromPoint(pt)) {
			m_wndTip.Cancel();
		}
	}
}
