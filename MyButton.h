#pragma once


// CMyButton

class CMyButton : public CButton
{
	DECLARE_DYNAMIC(CMyButton)

public:
	CMyButton();
	virtual ~CMyButton();
  bool m_bSelected;          // Flag that button is selected when draw is done
  COLORREF mSpecialColor;    // Set fill color if using special color, default light Green
  bool m_bShowSpecial;       // Flag to show background color
  bool m_bRightWasClicked;   // Flag set when right-button clicked, clear it if using it
  bool m_bNotifyOnDraws;     // Flag that it should send message when it draws

protected:

  void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
  void PreSubclassWindow();
  afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
  DECLARE_MESSAGE_MAP()
};


