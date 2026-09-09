////////////////////////////////////////////////////////////////
// Extracted from PixieLib and adapted for SerialEM. Original copyright notice:
//
// PixieLib(TM) Copyright 1997-2005 Paul DiLascia
// If this code works, it was written by Paul DiLascia.
// If not, I don't know who wrote it.
// Compiles with Visual Studio.NET 7.1 or greater. Set tabsize=3.
// 
// NOTE: PixieLib is NOT freeware! 
// If you didn't pay for your copy, you're violating my copyright!
//
#pragma once
#include <afxext.h>		// MFC control bars
#include <afxcmn.h>		// MFC tooltip ctrl


//////////////////
// Popup text window is similar to a tooltip but eaiser to use. Just
// instantiate and Create. For example:
//
//		m_wndTip.Create(CPoint(0,0), pParent);		// create popup text
//		m_wndTip.m_szMargins = CSize(4,0);			// set margins
//		m_wndTip.SetWindowText("hello, world");	// set tip text
//		m_wndTip.ShowDelayed(200);						// show in 200 milliseconds
//
// To cancel (hide) the tip:
//
//		m_wndTip.Cancel();
//
// Used in CMenuTipManager to display menu tips. You can change m_font and/or
// m_color for different font/color (default=tooltip font/color), or override
// virtual DrawText function to do custom drawing.
//
class CPopupText : public CWnd {
public:
	// change these if you like:
	CSize m_szMargins;		// extra space around text
	COLORREF m_clrBG;			// background color (default: tooltip bg color)
	COLORREF m_clrText;		// text color (default: tooltip text color)
	CFont m_font;				// font (default: menu font)
	UINT m_dfltDrawFlags;	// default flags for drawtext, you can change

	CPopupText();
	virtual ~CPopupText();

	BOOL Create(CPoint pt, CWnd* pParentWnd, UINT nID=0);
	void ShowDelayed(UINT msec, UINT msecTimeout=0);
	void Cancel();

protected:
	UINT  m_msecTimeout;		// auto-timeout

	virtual void PostNcDestroy();
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

	virtual void DrawText(CDC& dc, LPCTSTR lpText, CRect& rc, UINT flags);

	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT OnSetText(WPARAM wp, LPARAM lp);

	DECLARE_DYNAMIC(CPopupText);
	DECLARE_MESSAGE_MAP();
};

//////////////////
// Implements menu tips for MFC main window. Menu tips are tooltips that
// appear next to the menu item when you hold the mouse over it. To use:
// instantiate in your CMainFrm, then call Install from your OnCreate handler.
//
//		// in CMainFrame::OnCreate
//		m_menuTipManager.Install(this);
//
// You also have to implement prompt strings for each menu item ID the normal
// MFC way: as resource strings with the same command ID. If you're using MFC,
// you should already have done this.
//
class CMenuTipManager : public CSubclassWnd {
protected:
	CPopupText m_wndTip;		// home-grown "tooltip"
	BOOL m_bMouseSelect;		// whether menu invoked by mouse
	BOOL m_bSticky;			// after first tip appears, show rest immediately

public:
	int m_iDelay;				// tooltip delay: you can change

	CMenuTipManager() : m_iDelay(1000), m_bSticky(FALSE) { }
	~CMenuTipManager() { }

	// call this to install tips
	void Install(CWnd* pWnd) { HookWindow(pWnd); }

	// Useful helpers to get window/rect of current active menu
	static CWnd* GetRunningMenuWnd();
	//static void  GetRunningMenuRect(CRect& rcMenu);
	CRect GetMenuTipRect(HMENU hmenu, UINT nID);

	// Useful helper to get the prompt string for a command ID.
	// Like CFrameWnd::GetMessageString, but you don't need a frame wnd.
	static CString GetResCommandPrompt(UINT nID);

	// Get the prompt for given command ID
	virtual CString OnGetCommandPrompt(UINT nID)
	{
		return GetResCommandPrompt(nID);
	}

	// hook fn to trap main window's messages
	virtual LRESULT WindowProc(UINT msg, WPARAM wp, LPARAM lp);

	// Call these handlers from your main window
	void OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hMenu);
	void OnEnterIdle(WPARAM nWhy, HWND hwndWho);
};