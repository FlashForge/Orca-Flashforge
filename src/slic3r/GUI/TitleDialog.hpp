#ifndef slic3r_GUI_TitleDialog_hpp_
#define slic3r_GUI_TitleDialog_hpp_
#include <wx/window.h>
#include "GUI_Utils.hpp"
//#include "ImageButton.hpp"


namespace Slic3r::GUI {

class TitleBar : public wxWindow
{
public:
    TitleBar(wxWindow *parent, const wxString& title, const wxColour& color, int borderRadius = 6);

    wxSize DoGetBestClientSize() const override;
    //void SetBackgroundColor(const wxColour& color);

    void SetTitle(const wxString& title);

protected:
    void OnPaint(wxPaintEvent& event);
    void OnMouseLeftDown(wxMouseEvent &event);
    void OnMouseLeftUp(wxMouseEvent &event);
    void OnMouseMotion(wxMouseEvent &event);
    void OnMouseCaptureLost(wxMouseCaptureLostEvent& event);
    void OnClose(wxCommandEvent &event);
    void FinishDrag();

private:
    bool        m_dragging;
    int         m_borderRadius;
    wxColour    m_bgColor;
    wxString    m_title;
    wxPoint     m_dragStartMouse;
    wxPoint     m_dragStartWindow;
    wxStaticText*   m_titleLbl;
    wxBitmapButton* m_closeBtn;
};


// TitleDialog
class TitleDialog : public DPIDialog
{
public:
    TitleDialog(wxWindow* parent, const wxString& title, int borderRadius = 6);

    void SetTitleBackgroundColor(const wxColour& color);

    wxBoxSizer* MainSizer();

protected:    
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    
protected:
    int             m_borderRadius;
    TitleBar*       m_titleBar;
    wxBoxSizer*     m_mainSizer;
};

} // Slic3r::GUI

#endif /* slic3r_GUI_TitleDialog_hpp_ */