#ifndef slic3r_GUI_FFTransientWindow_hpp_
#define slic3r_GUI_FFTransientWindow_hpp_

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/popupwin.h>

namespace Slic3r { namespace GUI {

class FFRoundedWindow : public wxPopupWindow
{
public:
    FFRoundedWindow(wxWindow *parent);

protected:
    void OnSize(wxSizeEvent &evt);

    void OnPaint(wxPaintEvent &evt);

protected:
    const int m_radius;
};

class FFTransientWindow : public FFRoundedWindow
{
public:
    FFTransientWindow(wxWindow *parent, wxString titleText);

    bool Show(bool show = true);

    wxBoxSizer *MainSizer() { return m_mainSizer; }

protected:
    void OnLeftDown(wxMouseEvent &evt);

    void OnMotion(wxMouseEvent &evt);

    void OnMouseCaptureLost(wxMouseCaptureLostEvent &evt);

    void OnActivateApp(wxActivateEvent& event);

    void SetHoverClose(bool isHover);

private:
    wxPanel        *m_titlePanel;
    wxStaticText   *m_titleLbl;
    wxBitmap        m_closeBmp;
    wxBitmap        m_closeHoverBmp;
    bool            m_isHoverClose;
    wxStaticBitmap *m_closeStaticBmp;
    wxBoxSizer     *m_mainSizer;
};

}} // namespace Slic3r::GUI

#endif
