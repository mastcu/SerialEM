// XSliderCtrl.h : header file
//
#ifndef XSLIDER_CTRL_H
#define XSLIDER_CTRL_H

class CXSliderCtrl : public CSliderCtrl
{
// Construction
public:
        CXSliderCtrl();
        void SetColors(COLORREF cr, COLORREF crHighlight);

// Attributes
public:

// Operations
public:

// Overrides
        // ClassWizard generated virtual function overrides
        //{{AFX_VIRTUAL(CXSliderCtrl)
        //}}AFX_VIRTUAL

// Implementation
public:
        virtual ~CXSliderCtrl();

        // Generated message map functions
protected:

        CPen m_Pen;
        COLORREF mIdleCR;
        COLORREF mHighlightCR;
        float mTrimFrac;

        //{{AFX_MSG(CXSliderCtrl)
        afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
        //}}AFX_MSG

        DECLARE_MESSAGE_MAP()
};
#endif
