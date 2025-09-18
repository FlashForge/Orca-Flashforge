#ifndef slic3r_GUI_ReLoginDialog_hpp_
#define slic3r_GUI_ReLoginDialog_hpp_

#include <wx/wx.h>
#include <wx/intl.h>

#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"
#include "slic3r/GUI/Widgets/WebView.hpp"
#include "slic3r/GUI/TitleDialog.hpp"

namespace Slic3r { 
namespace GUI {

class RoundImage : public wxPanel
{
public:
    RoundImage(wxWindow *parent, const wxSize &size = wxDefaultSize);

    void SetImage(const wxImage image);

private:
    void OnPaint(wxPaintEvent &event);
    void OnSize(wxSizeEvent &event);
    void CreateRegion(wxDC &dc);

private:
    wxImage m_image{wxNullImage};
};

class ReLoginDialog : public TitleDialog
{
public:
    ReLoginDialog();
    virtual ~ReLoginDialog();

private:
#if defined(__WIN32__) || defined(__LINUX__)
    void onLoginoutBtnClicked(wxCommandEvent& event);
#elif defined(__APPLE__)
    void  onLoginoutBtnClicked(wxMouseEvent &event);
#endif
    void onRelogin2BtnClicked(wxMouseEvent& event);
    void onUserImageUpdated(wxCommandEvent& event);

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;

private:
    wxBoxSizer *m_sizer_main{nullptr};
    wxPanel* m_panel_page{nullptr};

    wxStaticText *m_usr_name{nullptr};
    RoundImage  *m_user_panel;

    FFButton* m_re_login_button {nullptr};
#if defined(__WIN32__) || defined(__LINUX__)
    wxButton* m_login_out_button {nullptr};
#elif defined(__APPLE__)
    FFButton *m_login_out_button{nullptr};
#endif
};
} // namespace GUI
} // namespace Slic3r

#endif
