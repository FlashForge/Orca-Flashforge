#ifndef slic3r_GUI_TitleDialog_hpp_
#define slic3r_GUI_TitleDialog_hpp_
#include <wx/window.h>
#include <wx/dc.h>
#include "GUI_Utils.hpp"
#include <wx/wx.h>

namespace Slic3r::GUI {

class TitleBar : public wxWindow
{
public:
    TitleBar(wxWindow *parent, const wxString& title, const wxColour& color, int borderRadius = 6);

    wxSize DoGetBestClientSize() const override;

    void SetTitle(const wxString& title);

protected:
    void OnPaint(wxPaintEvent& event);
    void DoRender(wxDC &dc);
    void OnMouseLeftDown(wxMouseEvent &event);
    void OnMouseLeftUp(wxMouseEvent &event);
    void OnMouseMotion(wxMouseEvent &event);
    void OnMouseCaptureLost(wxMouseCaptureLostEvent& event);
    void OnCloseClicked(wxMouseEvent& event);
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

class TitleDialog : public DPIDialog
{
public:
    TitleDialog(wxWindow* parent, const wxString& title, int borderRadius = 6, const wxSize &size = wxDefaultSize);

    wxBoxSizer* MainSizer();

    void SetTitleBackgroundColor(const wxColour& color);
    void SetSize(const wxSize& size);
    wxSize GetSize() const;

protected:
    void on_dpi_changed(const wxRect &suggested_rect) {}
    void OnPaint(wxPaintEvent& event);
    void DoRender(wxDC &dc);
    void OnSize(wxSizeEvent& event);

protected:
    int             m_borderRadius {6};
    const int       m_shadow_width {1};
    TitleBar*       m_titleBar {nullptr};
    wxBoxSizer*     m_mainSizer {nullptr};
};

} // Slic3r::GUI

#endif /* slic3r_GUI_TitleDialog_hpp_ */
