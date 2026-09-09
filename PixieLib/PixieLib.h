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
// This is the main include file for PixieLib. Applications that use
// PixieLib should include this file in stdafx.h.
//
#pragma once


//////////////////
// Generic class to subclass a CWnd. Once subclassed, all messages go to
// CSubclassWnd::WindowProc before going to the window. Specific subclasses
// can trap messages and do something. This very powerful class lets you
// write a plug-in classes that handle windows messages without deriving a
// new window class. To use:
//
// * Derive from CSubclassWnd.
//
// * Override CSubclassWnd::WindowProc to handle messages. Make sure you call
//   CSubclassWnd::WindowProc if you don't handle the message, or your
//   window will never get messages. If you write separate message handlers,
//   you can call Default() to pass the message to the window.
//
// * Instantiate your derived class somewhere and call HookWindow(pWnd)
//   to hook your window, AFTER it has been created.
//	  To unhook, call Unhook or HookWindow(NULL).
//
// Many PixieLib classes derive from this very important class. To see how it
// works, look at the Hook sample program.
//
class CSubclassWnd : public CObject {
public:
	CSubclassWnd();
	~CSubclassWnd();

	// Subclass a window. Hook(NULL) to unhook (automatic on WM_NCDESTROY)
	BOOL	HookWindow(HWND  hwnd);
	BOOL	HookWindow(CWnd* pWnd)	{ return HookWindow(pWnd->GetSafeHwnd()); }
	void	Unhook()						{ HookWindow((HWND)NULL); }
	BOOL	IsHooked()					{ return m_hWnd!=NULL; }

	friend LRESULT CALLBACK HookWndProc(HWND, UINT, WPARAM, LPARAM);
	friend class CSubclassWndMap;

	virtual LRESULT WindowProc(UINT msg, WPARAM wp, LPARAM lp);
	LRESULT Default();				// call this at the end of handler fns

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	HWND				m_hWnd;				// the window hooked
	WNDPROC			m_pOldWndProc;		// ..and original window proc
	CSubclassWnd*	m_pNext;				// next in chain of hooks for this window

	DECLARE_DYNAMIC(CSubclassWnd);
};

//////////////////
// Get NONCLIENTMETRICS info: ctor calls SystemParametersInfo. Just
// instantiate and use.
//
//		CNonClientMetrics ncm;
//		// now ncm is filled with NONCLIENTMETRICs
//
class CNonClientMetrics : public NONCLIENTMETRICS {
public:
	CNonClientMetrics() {
		cbSize = sizeof(NONCLIENTMETRICS);
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS,0,this,0);
	}
};

#include "PLCoolUI.h"
