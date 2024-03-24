#ifndef slic3r_GUI_ReLoginDialog_hpp_
#define slic3r_GUI_ReLoginDialog_hpp_

#include <wx/wx.h>
#include <wx/intl.h>
#include <wx/webview.h>
#include <wx/webrequest.h>

#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"
#include "slic3r/GUI/Widgets/WebView.hpp"
#include "slic3r/GUI/TitleDialog.hpp"

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

    void onReloginBtnClicked(wxCommandEvent& event);
    void onLoginoutBtnClicked(wxCommandEvent& event);

    void onRelogin2BtnClicked(wxMouseEvent& event);

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;

private:
    void onCloseWnd(wxCloseEvent &event);
    void onDestroy();

private:

    wxBoxSizer *m_sizer_main{nullptr};
    wxPanel* m_panel_page{nullptr};

    wxBitmap m_usr_pic;
    //Label* m_usr_name{nullptr};
    wxStaticText *m_usr_name{nullptr};
    wxWebRequest m_web_request;
    RoundImage  *m_user_panel;

    wxStaticBitmap*  m_usr_pic_staticbitmap;

    FFButton* m_re_login_button {nullptr};
    wxButton* m_login_out_button {nullptr};

    wxWebView * m_user_pic_view {nullptr};

};
} // namespace GUI
} // namespace Slic3r

#endif