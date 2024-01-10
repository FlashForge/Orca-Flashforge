#ifndef slic3r_GUI_ReLoginDialog_hpp_
#define slic3r_GUI_ReLoginDialog_hpp_

#include <wx/wx.h>
#include <wx/intl.h>
#include <wx/webview.h>

#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/WebView.hpp"

#if wxUSE_WEBVIEW_IE
#include "wx/msw/webview_ie.h"
#endif
#if wxUSE_WEBVIEW_EDGE
#include "wx/msw/webview_edge.h"
#endif

#include "wx/webviewarchivehandler.h"
#include "wx/webviewfshandler.h"

namespace Slic3r { 
namespace GUI {
class ReLoginDialog : public DPIDialog
{
public:
    ReLoginDialog();
    virtual ~ReLoginDialog();

    void onReloginBtnClicked(wxCommandEvent& event);
    void onLoginoutBtnClicked(wxCommandEvent& event);

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;

private:
    wxPanel* m_panel_page{nullptr};

    wxBitmap m_usr_pic;
    Label* m_usr_name{nullptr};

    wxStaticBitmap*  m_usr_pic_staticbitmap;

    wxButton* m_re_login_button {nullptr};
    wxButton* m_login_out_button {nullptr};

    wxWebView * m_user_pic_view {nullptr};

};
} // namespace GUI
} // namespace Slic3r

#endif