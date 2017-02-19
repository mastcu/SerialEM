/****************************************************

   Set dialog DPI helper class

   Use this class to make your dialog-editor based 
   dialogs look the same under any Windows DPI 
   settings (Large font mode).
   
   This is important if your dialogs have bitmaps on 
   them. You can specify current DPI if you want 
   bitmaps to get resized to current DPI (normally
   they don't get resized).

   Copyright (C)2003 by George Yohng 
   http://www.yohng.com

   LICENSE AGREEMENT:

      You agree to go to http://www.yohng.com/music.html
      and listen completely (without skips) to the very
      first track on the list.

 ****************************************************/

/*  

 *************************************************
 * Warning: for proper sizing with large fonts,  *
 *          set dialog font explicitly to        *
 *                                               *
 *         Microsoft Sans Serif (as opposed to   *
 *                                MS Sans Serif) *
 *          or Tahoma                            *
 *                                               *
 *          (they have the same sizes)           *
 *************************************************

   // -------------------------------------------------------------------
   // MFC Example:
   // -------------------------------------------------------------------

   ...

   BOOL CMyDlg::OnInitDialog()
   {
       CDialog::OnInitDialog();

       dpi.Attach(AfxFindResourceHandle(IMAKEINTRESOURCE(IDD), RT_DIALOG),
                  m_hWnd,IDD,96.0); // 96 is the DPI

       // The rest of your initialization code goes here

       return TRUE;
   }

   // -------------------------------------------------------------------
   // ATL/WTL Example:
   // -------------------------------------------------------------------

   ...

   BOOL CMyDlg::OnInitDialog()
   {
       CDialog::OnInitDialog();

       dpi.Attach(_AtlBaseModule.GetResourceInstance(),m_hWnd,IDD,96.0);
       //                                                         ^^^^ DPI

       // The rest of your initialization code goes here

       return TRUE;
   }

*/

#ifndef SETDPI_INCLUDED
#define SETDPI_INCLUDED

class CSetDPI
{
public:
    int IDD;
    HINSTANCE inst;
    HWND hwnd;
    HFONT font,oldfont;

    CSetDPI();

    void Attach(HINSTANCE inst,HWND dlg,int IDD,double dpi);

    void Detach();

    ~CSetDPI();
};

#endif
