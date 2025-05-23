#include "ReLoginDialog.hpp"
#include "slic3r/GUI/I18N.hpp"

#include "libslic3r/Utils.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include "slic3r/GUI/format.hpp"
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"
#include "slic3r/GUI/FlashForge/LoginDialog.hpp"
#include "slic3r/GUI/FlashForge/DeviceData.hpp"
#include "slic3r/GUI/FFUtils.hpp"

namespace Slic3r {
namespace GUI {
wxDEFINE_EVENT(EVT_DOWNLOADED_RELOGIN_IMAGE, StdStringEvent);
RoundImage::RoundImage(wxWindow *parent, const wxSize &size /*=wxDefaultSize*/)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
{
    Bind(wxEVT_PAINT, &RoundImage::OnPaint, this);
    // Bind(wxEVT_SIZE, &RoundImagePanel::OnSize, this);
}

void RoundImage::SetImage(const wxImage image)
{
    m_image = image;
    Refresh();
}

void RoundImage::OnSize(wxSizeEvent &event) {}

void RoundImage::OnPaint(wxPaintEvent &event)
{
    if (!m_image.IsOk()) {
        event.Skip();
        return;
    }

    wxSize  size = GetSize();
    wxImage img  = m_image;
    img.Rescale(size.x, size.y);
    wxPaintDC dc(this);
    if (!img.HasAlpha()) {
        img.InitAlpha();
    }
    wxBitmap bmp(size.x, size.y);
    {
        wxMemoryDC memdc;
        memdc.SelectObject(bmp);
#ifdef _WIN32
        memdc.Blit({0, 0}, size, &dc, {0, 0});
#endif
        wxGCDC dc2(memdc);
        dc2.SetFont(GetFont());
        CreateRegion(dc2);
        memdc.SelectObject(wxNullBitmap);
    }
    wxImage ref_img = bmp.ConvertToImage();
    for (int y = 0; y < img.GetHeight(); ++y) {
        for (int x = 0; x < img.GetWidth(); ++x) {
            img.SetAlpha(x, y, ref_img.GetRed(x, y));
        }
    }
    dc.DrawBitmap(wxBitmap(img), 0, 0);
}

void RoundImage::CreateRegion(wxDC &dc)
{
    wxSize sz = GetSize();
    int    x  = sz.x / 2;
    int    y  = sz.y / 2;
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(0, 0, sz.x, sz.y);
    //dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetPen(wxColor("#000000"));
    dc.SetBrush(*wxRED);
    dc.DrawCircle(x, y, (x < y) ? x : y);
}

ReLoginDialog::ReLoginDialog() : TitleDialog(static_cast<wxWindow *>(wxGetApp().mainframe), _L("Login"), 6)
{
    SetFont(wxGetApp().normal_font());
    SetBackgroundColour(*wxWHITE);

    AppConfig *app_config = wxGetApp().app_config;
    std::string usr_name("usrname");
    std::string pic = "login_default_usr_pic.png";
    std::string usr_pic((boost::filesystem::path(Slic3r::var_dir()) / pic).make_preferred().string());
    if(app_config){
        usr_name = app_config->get("usr_name");
        if(!app_config->get("usr_pic").empty()){
            usr_pic = app_config->get("usr_pic");
        }
    }

//水平布局
    m_sizer_main = MainSizer();
    m_sizer_main->SetMinSize(wxSize(FromDIP(380), FromDIP(445)));

//***添加空白间距
    auto m_panel_separotor_0 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1,FromDIP(83)), wxTAB_TRAVERSAL);
    m_panel_separotor_0->SetBackgroundColour(wxColour(255,255,255));

    m_sizer_main->Add(m_panel_separotor_0, 0, wxEXPAND | wxALL, 0);

//**添加用户
    #if 0
    m_web_request = wxWebSession::GetDefault().CreateRequest(this, usr_pic);
    if (!m_web_request.IsOk()) {
        BOOST_LOG_TRIVIAL(error) << "web session create request fail";
    } else {
        m_web_request.Start();
    }
    #else
    #if 1
    m_user_panel = new RoundImage(this, wxSize(FromDIP(80), FromDIP(80)));

    wxImage usr_image = wxGetApp().getUsrPic();
    if (usr_image.IsOk()) {
        usr_image.Rescale(FromDIP(80), FromDIP(80));
        m_user_panel->SetImage(usr_image);
        Layout();
    } else {
        downloadUrlPic(usr_pic);
    }        
     #endif
    #endif

    Bind(wxEVT_CLOSE_WINDOW, &ReLoginDialog::onCloseWnd, this);
    Bind(EVT_DOWNLOADED_RELOGIN_IMAGE, [&](StdStringEvent& event) {
        std::string         body = event.str;
        wxMemoryInputStream stream(body.data(), body.size());
        wxImage             image(stream, wxBITMAP_TYPE_ANY);
        if (!image.IsOk()) {
            BOOST_LOG_TRIVIAL(error) << "download relogin image is not ok";
            return;
        }
        wxGetApp().setUsrPic(image);
        image.Rescale(FromDIP(80), FromDIP(80));
        if (m_user_panel) {
            m_user_panel->SetImage(image);
        }
        Layout();
    });
    #if 0
    Bind(wxEVT_WEBREQUEST_STATE, [this](wxWebRequestEvent &evt) {
        switch (evt.GetState()) {
        case wxWebRequest::State_Completed: {
            BOOST_LOG_TRIVIAL(error) << "BindDialog: web request state completed";
            wxImage avatar_stream = *evt.GetResponse().GetStream();
            if (avatar_stream.IsOk()) {
                avatar_stream.Rescale(FromDIP(80), FromDIP(80));
                m_user_panel->SetImage(avatar_stream);
                Layout();
            }
            break;
        }
        case wxWebRequest::State_Failed: {
            BOOST_LOG_TRIVIAL(error) << "BindDialog: web request state failed";
            break;
        }
        }
    });
    #endif
    

    m_sizer_main->Add(m_user_panel, 0, wxALIGN_CENTER, 0);
    m_sizer_main->AddSpacer(FromDIP(6));

    wxString username = wxString::FromUTF8(usr_name);
    wxGCDC   dc(this);
    wxString clipName = FFUtils::trimString(dc, username, FromDIP(180));
    m_usr_name = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize);
    m_usr_name->SetLabelText(clipName);
    m_usr_name->SetToolTip(wxString::FromUTF8(usr_name));

    m_sizer_main->Add(m_usr_name, 0, wxALIGN_CENTER, 0);

//***添加空白间距
    auto m_panel_separotor_1 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1,FromDIP(29)), wxTAB_TRAVERSAL);
    m_panel_separotor_1->SetBackgroundColour(wxColour(255,255,255));

    m_sizer_main->Add(m_panel_separotor_1, 0, wxEXPAND | wxALL, 0);

//**登录按钮
    m_re_login_button = new FFButton(this, wxID_ANY,_L("Login"),8);

    m_re_login_button->SetFontDisableColor(wxColour(255, 255, 255));
    m_re_login_button->SetBorderDisableColor(wxColour(221,221,221));
    m_re_login_button->SetBGColor(wxColour(221,221,221));

    m_re_login_button->SetFontHoverColor(wxColour(255, 255, 255));
    m_re_login_button->SetBGHoverColor(wxColour(149,197,255));
    m_re_login_button->SetBorderHoverColor(wxColour(149,197,255));

    m_re_login_button->SetFontPressColor(wxColour(255, 255, 255));
    m_re_login_button->SetBGPressColor(wxColour(17,111,223));
    m_re_login_button->SetBorderPressColor(wxColour(17,111,223));

    m_re_login_button->SetFontColor(wxColour(255, 255, 255));
    m_re_login_button->SetBorderColor(wxColour(50,141,251));
    m_re_login_button->SetBGColor(wxColour(50,141,251));
    m_re_login_button->Bind(wxEVT_LEFT_UP,&ReLoginDialog::onRelogin2BtnClicked, this);
    m_re_login_button->SetMinSize(wxSize(FromDIP(170),FromDIP(35)));
    //m_re_login_button->SetFont((wxFont(wxFontInfo(16))));

    m_sizer_main->Add(m_re_login_button, 0, wxALIGN_CENTER, 0);

//***添加空白间距
    auto m_panel_separotor_2 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1,FromDIP(18)), wxTAB_TRAVERSAL);
    m_panel_separotor_2->SetBackgroundColour(wxColour(255,255,255));

    m_sizer_main->Add(m_panel_separotor_2, 0, wxEXPAND | wxALL, 0);

//**登出按钮
#if defined(__WIN32__) || defined(__LINUX__)
    m_login_out_button = new wxButton(this, wxID_ANY, _L("Login out"));
    m_login_out_button->SetForegroundColour(wxColour(50,141,251));
    m_login_out_button->SetBackgroundColour(wxColour(255,255,255)); 
    m_login_out_button->SetWindowStyleFlag(wxBORDER_NONE); 
    //m_login_out_button->SetFont((wxFont(wxFontInfo(16))));
    m_login_out_button->Bind(wxEVT_BUTTON,&ReLoginDialog::onLoginoutBtnClicked, this);
#elif defined(__APPLE__)
    m_login_out_button = new FFButton(this, wxID_ANY, _L("Login out"),8);

    m_login_out_button->SetFontDisableColor(wxColour(50, 141, 251));
    m_login_out_button->SetBorderDisableColor(wxColour(255, 255, 255));
    m_login_out_button->SetBGColor(wxColour(255, 255, 255));

    m_login_out_button->SetFontHoverColor(wxColour(50, 141, 251));
    m_login_out_button->SetBGHoverColor(wxColour(255, 255, 255));
    m_login_out_button->SetBorderHoverColor(wxColour(255, 255, 255));

    m_login_out_button->SetFontPressColor(wxColour(50, 141, 251));
    m_login_out_button->SetBGPressColor(wxColour(255, 255, 255));
    m_login_out_button->SetBorderPressColor(wxColour(255, 255, 255));

    m_login_out_button->SetFontColor(wxColour(50, 141, 251));
    m_login_out_button->SetBorderColor(wxColour(255, 255, 255));
    m_login_out_button->SetBGColor(wxColour(255, 255, 255));
    m_login_out_button->Bind(wxEVT_LEFT_UP, &ReLoginDialog::onLoginoutBtnClicked, this);
    m_login_out_button->SetMinSize(wxSize(FromDIP(170), FromDIP(35)));
#endif

    m_sizer_main->Add(m_login_out_button, 0, wxALIGN_CENTER, 0);

    Fit();
    //Thaw();
    Centre(wxBOTH);
    Layout();
}

ReLoginDialog::~ReLoginDialog()
{
}
#if defined(__WIN32__) || defined(__LINUX__)
void ReLoginDialog::onLoginoutBtnClicked(wxCommandEvent& event)
{
#if defined(__WIN32__) || defined(__LINUX__)
    Hide();
#elif defined(__APPLE__)
    Close();
#endif
    wxGetApp().handle_login_out();
    AppConfig *app_config = wxGetApp().app_config;
    if(app_config){
        std::string access_token = app_config->get("access_token");
        if(!access_token.empty()){
            ComErrno login_out_result = MultiComUtils::signOut(access_token, ComTimeoutWanA);
            if(login_out_result != ComErrno::COM_OK){
                BOOST_LOG_TRIVIAL(warning) << boost::format("MultiComUtils::signOut Failed!");
            }
            //DeviceObjectOpr *devOpr = wxGetApp().getDeviceObjectOpr();
            //devOpr->clear_user_machine();
        }
        
        app_config->set("access_token","");
        app_config->set("refresh_token","");
        app_config->set("token_expire_time", "");
        app_config->set("token_start_time", "");
        app_config->set("usr_name","");
        app_config->set("usr_pic","");
        Slic3r::GUI::MultiComMgr::inst()->removeWanDev();
    }
    event.Skip();
}
#endif

#ifdef __APPLE__
void ReLoginDialog::onLoginoutBtnClicked(wxMouseEvent &event) 
{
#if defined(__WIN32__) || defined(__LINUX__)
    Hide();
#elif defined(__APPLE__)
    Close();
#endif
    wxGetApp().handle_login_out();
    AppConfig *app_config = wxGetApp().app_config;
    if (app_config) {
        std::string access_token = app_config->get("access_token");
        if (!access_token.empty()) {
            ComErrno login_out_result = MultiComUtils::signOut(access_token, ComTimeoutWanA);
            if (login_out_result != ComErrno::COM_OK) {
                BOOST_LOG_TRIVIAL(warning) << boost::format("MultiComUtils::signOut Failed!");
            }
            // DeviceObjectOpr *devOpr = wxGetApp().getDeviceObjectOpr();
            // devOpr->clear_user_machine();
        }

        app_config->set("access_token", "");
        app_config->set("refresh_token", "");
        app_config->set("token_expire_time", "");
        app_config->set("token_start_time", "");
        app_config->set("usr_name", "");
        app_config->set("usr_pic", "");
        Slic3r::GUI::MultiComMgr::inst()->removeWanDev();
    }
    event.Skip();
}
#endif

void ReLoginDialog::onRelogin2BtnClicked(wxMouseEvent& event)
{
#if defined(__WIN32__) || defined(__LINUX__)
    Hide();
#elif defined(__APPLE__)
    Close();
#endif
    AppConfig *app_config = wxGetApp().app_config;
    if(app_config){
        std::string usr_name = app_config->get("usr_name");
        std::string usr_pic = app_config->get("usr_pic");
        if (usr_name.empty()) {
            usr_name = LoginDialog::GetUsrName();
        }
        std::string refresh_token = app_config->get("refresh_token");
        std::string access_token = app_config->get("access_token");
        if(refresh_token.empty() && access_token.empty()){
            wxGetApp().ShowUserLogin();
        }
        else{
            wxGetApp().handle_login_result(usr_pic,usr_name);
            BOOST_LOG_TRIVIAL(info) << "usr login succeed 333 : ReLoginDialog::onRelogin2BtnClicked";
        }
    }
    event.Skip();
}

void ReLoginDialog::reLoad() 
{
    AppConfig  *app_config = wxGetApp().app_config;
    std::string usr_name("usrname");
    std::string pic = "login_default_usr_pic.png";
    std::string usr_pic((boost::filesystem::path(Slic3r::var_dir()) / pic).make_preferred().string());
    if (app_config) {
        usr_name = app_config->get("usr_name");
        if (!app_config->get("usr_pic").empty()) {
            usr_pic = app_config->get("usr_pic");
        }
    }

    wxString username = wxString::FromUTF8(usr_name);
    wxGCDC   dc(this);
    wxString clipName = FFUtils::trimString(dc, username, FromDIP(180));
    if (m_usr_name == NULL) {
        return;
    }
    m_usr_name->SetLabelText(clipName);
    m_usr_name->SetToolTip(wxString::FromUTF8(usr_name));

    if (m_user_panel == NULL) {
        return;
    }
    wxImage usr_image = wxGetApp().getUsrPic();
    if (usr_image.IsOk()) {
        usr_image.Rescale(FromDIP(80), FromDIP(80));
        m_user_panel->SetImage(usr_image);
        Layout();
    } else {
        downloadUrlPic(usr_pic);
    }
}

void ReLoginDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    ;
}

void ReLoginDialog::onCloseWnd(wxCloseEvent &event) 
{
    event.Skip();
}
void ReLoginDialog::downloadUrlPic(const std::string& url) 
{
    if (!url.empty()) {
        Slic3r::Http http   = Slic3r::Http::get(url);
        std::string  suffix = url.substr(url.find_last_of(".") + 1);
        http.header("accept", "image/" + suffix)
            .on_complete([this](std::string body, unsigned int status) { 
                auto event = new StdStringEvent;
                event->SetEventType(EVT_DOWNLOADED_RELOGIN_IMAGE);
                event->str = body;
                wxQueueEvent(this, event);
            })
            .on_error([=](std::string body, std::string error, unsigned status) {
                BOOST_LOG_TRIVIAL(info) << " ReLoginDialog::downloadUrlPic: status:" << status << " error:" << error;
            })
            .perform();
    } else {
        wxImage     tmpimage;
        std::string name = "login_default_usr_pic";
        if (tmpimage.LoadFile(Slic3r::GUI::from_u8(Slic3r::var(name + ".png")), wxBITMAP_TYPE_PNG)) {
            wxGetApp().setUsrPic(tmpimage);
            tmpimage.Rescale(FromDIP(80), FromDIP(80));
            if (m_user_panel) {
                m_user_panel->SetImage(tmpimage);
            }
            Layout();
        }
    }
}


}
}
