#include "ReLoginDialog.hpp"
#include "slic3r/GUI/I18N.hpp"

#include "libslic3r/Utils.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include "slic3r/GUI/format.hpp"
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"

namespace Slic3r {
namespace GUI {

ReLoginDialog::ReLoginDialog() : DPIDialog(static_cast<wxWindow *>(wxGetApp().mainframe), wxID_ANY, from_u8((boost::format(_utf8(_L("Login")))).str()))
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
    wxBoxSizer *bSizer_frame = new wxBoxSizer(wxHORIZONTAL);

//左侧间距
    auto m_panel_separotor_left = new wxPanel(this, wxID_ANY, wxDefaultPosition,wxSize(FromDIP(53), -1), wxTAB_TRAVERSAL);
    m_panel_separotor_left->SetBackgroundColour(wxColour(255,255,255));
    //m_panel_separotor_left->SetMinSize(wxSize(FromDIP(85), -1));

    bSizer_frame->Add(m_panel_separotor_left, 0, wxEXPAND | wxALL, 0);

//中间布局
    wxBoxSizer* bSizer_mid = new wxBoxSizer(wxVERTICAL);

    m_panel_page = new wxPanel(this, wxID_ANY,wxDefaultPosition, wxSize(FromDIP(300), FromDIP(430)), wxTAB_TRAVERSAL);
    m_panel_page->SetBackgroundColour(wxColour(255,255,255));

//***添加空白间距
    auto m_panel_separotor_0 = new wxPanel(m_panel_page, wxID_ANY, wxDefaultPosition, wxSize(-1,FromDIP(83)), wxTAB_TRAVERSAL);
    m_panel_separotor_0->SetBackgroundColour(wxColour(255,255,255));

    bSizer_mid->Add(m_panel_separotor_0, 0, wxEXPAND | wxALL, 0);

//**添加用户
    // m_usr_pic = create_scaled_bitmap("login_default_usr_pic", this, 66);
    // m_usr_pic_staticbitmap = new wxStaticBitmap(m_panel_page, wxID_ANY,m_usr_pic);
    // m_usr_pic_staticbitmap->SetMinSize(wxSize(64,64));

    m_user_pic_view = WebView::CreateWebView(m_panel_page, usr_pic);
    m_user_pic_view->SetMinSize(wxSize(64,64));

    bSizer_mid->Add(m_user_pic_view,0, wxALIGN_CENTER,0);

    //bSizer_mid->Add(m_usr_pic_staticbitmap,0, wxALIGN_CENTER,0);

    m_usr_name = new Label(m_panel_page,wxString::FromUTF8(usr_name.c_str()));

    //m_usr_name->Fit(m_panel_page);

    bSizer_mid->Add(m_usr_name,0, wxALIGN_CENTER,0);

//***添加空白间距
    auto m_panel_separotor_1 = new wxPanel(m_panel_page, wxID_ANY, wxDefaultPosition, wxSize(-1,FromDIP(29)), wxTAB_TRAVERSAL);
    m_panel_separotor_1->SetBackgroundColour(wxColour(255,255,255));

    bSizer_mid->Add(m_panel_separotor_1, 0, wxEXPAND | wxALL, 0);

//**登录按钮
    m_re_login_button = new FFButton(m_panel_page, wxID_ANY,_L("Login"));

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
    m_re_login_button->SetFont((wxFont(wxFontInfo(16))));

    bSizer_mid->Add(m_re_login_button,0, wxALIGN_CENTER,0);

//***添加空白间距
    auto m_panel_separotor_2 = new wxPanel(m_panel_page, wxID_ANY, wxDefaultPosition, wxSize(-1,FromDIP(18)), wxTAB_TRAVERSAL);
    m_panel_separotor_2->SetBackgroundColour(wxColour(255,255,255));

    bSizer_mid->Add(m_panel_separotor_2, 0, wxEXPAND | wxALL, 0);

//**登出按钮
    m_login_out_button = new wxButton(m_panel_page, wxID_ANY, _L("Login out"));
    m_login_out_button->SetForegroundColour(wxColour(50,141,251));
    m_login_out_button->SetBackgroundColour(wxColour(255,255,255)); 
    m_login_out_button->SetWindowStyleFlag(wxBORDER_NONE); 
    m_login_out_button->SetFont((wxFont(wxFontInfo(16))));
    m_login_out_button->Bind(wxEVT_BUTTON,&ReLoginDialog::onLoginoutBtnClicked, this);

    //m_login_out_button->Fit(m_panel_page);

    bSizer_mid->Add(m_login_out_button,0, wxALIGN_CENTER,0);

    bSizer_mid->AddStretchSpacer();

    //SetSizerAndFit(bSizer_mid);

    m_panel_page->SetSizer(bSizer_mid);
    m_panel_page->Layout();
    bSizer_mid->Fit(m_panel_page);

    bSizer_frame->Add(m_panel_page, 0, wxEXPAND | wxALL, 0);

//右侧间距
    auto m_panel_separotor_right = new wxPanel(this, wxID_ANY, wxDefaultPosition,wxSize(FromDIP(53), -1), wxTAB_TRAVERSAL);
    m_panel_separotor_right->SetBackgroundColour(wxColour(255,255,255));
    //m_panel_separotor_right->SetMinSize(wxSize(FromDIP(85), -1));

    bSizer_frame->Add(m_panel_separotor_right, 0, wxEXPAND | wxALL, 0);

    this->SetSizerAndFit(bSizer_frame);
    this->Layout();

}

ReLoginDialog::~ReLoginDialog()
{
    ;
} 

void ReLoginDialog::onReloginBtnClicked(wxCommandEvent& event)
{
    AppConfig *app_config = wxGetApp().app_config;
    if(app_config){
        std::string usr_name = app_config->get("usr_name");
        std::string usr_pic = app_config->get("usr_pic");
        wxGetApp().handle_login_result(usr_pic,usr_name);
    }
    Hide();
}

void ReLoginDialog::onLoginoutBtnClicked(wxCommandEvent& event)
{
    Hide();
    AppConfig *app_config = wxGetApp().app_config;
    if(app_config){
        std::string access_token = app_config->get("access_token");
        if(!access_token.empty()){
            ComErrno login_out_result = MultiComUtils::signOut(access_token);
            if(login_out_result != ComErrno::COM_OK){
                BOOST_LOG_TRIVIAL(warning) << boost::format("MultiComUtils::signOut Failed!");
            }
        }
        
        app_config->set("access_token","");
        app_config->set("refresh_token","");
        app_config->set("expire_time","");
        app_config->set("usr_name","");
        app_config->set("usr_pic","");
        Slic3r::GUI::MultiComMgr::inst()->removeWanDev();
    }
}

void ReLoginDialog::onRelogin2BtnClicked(wxMouseEvent& event)
{
    AppConfig *app_config = wxGetApp().app_config;
    if(app_config){
        std::string usr_name = app_config->get("usr_name");
        std::string usr_pic = app_config->get("usr_pic");
        wxGetApp().handle_login_result(usr_pic,usr_name);
    }
    Hide();
}

void ReLoginDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    ;
}

}
}