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

class StdStringEvent : public wxCommandEvent
{
public:
    std::string str;
};

wxDECLARE_EVENT(EVT_DOWNLOADED_RELOGIN_IMAGE, StdStringEvent);

class ReLoginDialog : public TitleDialog
{
public:
    ReLoginDialog();
    virtual ~ReLoginDialog();
#if defined(__WIN32__) || defined(__LINUX__)
    void onLoginoutBtnClicked(wxCommandEvent& event);
#elif defined(__APPLE__)
    void  onLoginoutBtnClicked(wxMouseEvent &event);
#endif
    void onRelogin2BtnClicked(wxMouseEvent& event);

    void reLoad();

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;

private:
    void onCloseWnd(wxCloseEvent &event);
    void downloadUrlPic(const std::string& url);

private:

    wxBoxSizer *m_sizer_main{nullptr};
    wxPanel* m_panel_page{nullptr};

    wxBitmap m_usr_pic;
    //Label* m_usr_name{nullptr};
    wxStaticText *m_usr_name{nullptr};
    //wxWebRequest m_web_request;
    RoundImage  *m_user_panel;

    wxStaticBitmap*  m_usr_pic_staticbitmap;

    FFButton* m_re_login_button {nullptr};
#if defined(__WIN32__) || defined(__LINUX__)
    wxButton* m_login_out_button {nullptr};
#elif defined(__APPLE__)
    FFButton *m_login_out_button{nullptr};
#endif

    wxWebView * m_user_pic_view {nullptr};
    std::vector<char> m_pic_data;
};
} // namespace GUI
} // namespace Slic3r

#endif
