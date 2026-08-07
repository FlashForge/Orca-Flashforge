#ifndef slic3r_GUI_TitleDialog_hpp_
#define slic3r_GUI_TitleDialog_hpp_
#include <wx/window.h>
#include <wx/dc.h>
#include "GUI_Utils.hpp"
#include <wx/wx.h>
#include "slic3r/GUI/Widgets/WebView.hpp"

namespace Slic3r {
namespace GUI {

class TitleBar : public wxWindow
{
public:
    TitleBar(wxWindow* parent, const wxString& title, const wxColour& color, int borderRadius = 6, bool titleCenter = true);
    void   SetBackgroundColor(wxColour color);
    wxSize DoGetBestClientSize() const override;
    void SetUnderLine(wxColour color, int width = 1);
    void SetTitle(const wxString& title);
    wxString GetTitle() const;

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
    wxColour    m_under_line_color;
    int         m_under_line_width{1};
    wxString    m_title;
    wxPoint     m_dragStartMouse;
    wxPoint     m_dragStartWindow;
    wxStaticText*   m_titleLbl;
    wxBitmapButton* m_closeBtn;
};

class TitleDialog : public DPIDialog
{
public:
    TitleDialog(wxWindow* parent, const wxString& title, int borderRadius = 6, const wxSize& size = wxDefaultSize, bool titleCenter = true);

    wxBoxSizer* MainSizer();
    TitleBar*   GetTitleBar();
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

class WebDialog : public TitleDialog
{
public:
    WebDialog(wxWindow*       parent,
              const wxString& title,
              const wxString& url,
              int             borderRadius = 6,
              const wxSize&   size         = wxDefaultSize,
              bool            titleCenter  = true);

private:
    wxWebView* m_browser{nullptr};
    void       OnNavigated(wxWebViewEvent& evt);
    void       OnNewWindow(wxWebViewEvent& evt);
};

}} // Slic3r::GUI

#endif /* slic3r_GUI_TitleDialog_hpp_ */
