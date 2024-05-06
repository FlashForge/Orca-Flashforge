#include "LoginDialog.hpp"

#include "slic3r/GUI/I18N.hpp"

#include "libslic3r/Utils.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include "slic3r/GUI/format.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/FFUtils.hpp"
#include "MultiComUtils.hpp"
#include "MultiComMgr.hpp"

#include <wx/clipbrd.h>
#include <wx/dcgraph.h>

#include <regex>
#include <wx/regex.h>
#include <wx/string.h>

namespace Slic3r {
namespace GUI {

    wxDEFINE_EVENT(EVT_UPDATE_TEXT_LOGIN, wxCommandEvent);

    com_token_data_t LoginDialog::m_token_data = {};
    bool LoginDialog::m_usr_is_login = false;
    com_user_profile_t LoginDialog::m_usr_info = {};
    std::string  LoginDialog::m_usr_name = "";
    bool LoginDialog::m_first_call_client_token = true;
    std::string  serverLanguageEn = "en";
    std::string  serverLanguageZh = "zh";

    CountdownButton::CountdownButton(wxWindow* parent, wxString text, wxString icon /*= ""*/, long style /*= 0*/, int iconSize /*= 0*/, wxWindowID btn_id /*= wxID_ANY*/)
        : FFButton(parent,wxID_ANY,text,10)
        , m_countdown(60)
    {
        SetBackgroundColour(*wxWHITE);
        m_timer.Bind(wxEVT_TIMER, &CountdownButton::OnTimer, this);
    }

    void CountdownButton::OnTimer(wxTimerEvent& event)
    {
        event.Skip();
        std::lock_guard lock(m_mutex);
        m_countdown--;
        if (m_countdown > 0)
        {
            SetLabel(wxString::Format("%d s", m_countdown));
            Refresh();
        }
        else
        {
            Enable(true);
            SetEnable(true);
            m_timer.Stop();
            m_countdown = 60;

            SetLabel(_L("Get Code"));
            m_press = false;
            Refresh();
        }
    }

    void CountdownButton::SetState(bool state)
    {
        m_press = state;
    }

    bool CountdownButton::GetState()
    {
        return m_press;
    }

void CountdownButton::StopTimer()
    {
    Enable(true);
    SetEnable(true);
    m_timer.Stop();
    m_countdown = 60;

    SetLabel(_L("Get Code"));
    m_press = false;
    Refresh();
}

LoginDialog::LoginDialog() : TitleDialog(static_cast<wxWindow *>(wxGetApp().mainframe), _L("Login"), 6), m_cur_language("en")
{
    SetFont(wxGetApp().normal_font());
	SetBackgroundColour(*wxWHITE);
 
    AppConfig *app_config = wxGetApp().app_config;
    std::string region = app_config->get("region");
    if(region.compare("China") == 0){
        initWidget();
        initBindEvent();
        initData();
    }
    else{
        initOverseaWidget();
        ComErrno get_result = MultiComUtils::getClientToken(m_client_SMS_token);
        if(get_result == ComErrno::COM_ERROR){
            BOOST_LOG_TRIVIAL(warning) << boost::format("MultiComUtils::getClientToken Failed!");
        }
    }
    m_cur_language = app_config->get("language");
    
    this->SetMinSize(wxSize(FromDIP(330), FromDIP(439)));
}

LoginDialog::~LoginDialog() 
{
    m_first_call_client_token = true;
}

void LoginDialog::ReLoad()
{
    switchTitle1(); 
    m_username_ctrl_page1->ClearTxt();
    m_verifycode_ctrl_page1->ClearTxt();
    m_username_ctrl_page2->ClearTxt();
    m_password_ctrl_page2->ClearTxt();
    m_password_ctrl_page2->ShowEncrypt();
    m_get_code_button->StopTimer();
    m_get_code_button->Disable();
    m_page1_checkBox->SetValue(false);
    m_page2_checkBox->SetValue(false);
}

com_token_data_t LoginDialog::GetLoginToken()
{
    return m_token_data;
}

void LoginDialog::SetToken(const std::string& accessToken, const std::string& refreshToken)
{
    m_token_data.accessToken = accessToken;
    m_token_data.refreshToken = refreshToken;
}

void LoginDialog::SetUsrLogin(bool loginState)
{
    m_usr_is_login = loginState;
}

bool LoginDialog::IsUsrLogin()
{
    return m_usr_is_login;
}

void LoginDialog::SetUsrInfo(const com_user_profile_t& usrInfo)
{
    m_usr_info = usrInfo;
}

const com_user_profile_t& LoginDialog::GetUsrInfo()
{
    return m_usr_info;
}

const std::string LoginDialog::GetUsrName() 
{
    return m_usr_name;
}

void LoginDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    const int& em = em_unit();
    const wxSize& size = wxSize(65 * em, 500 * em);
    SetMinSize(size);
    Fit();
    Refresh();
}

void LoginDialog::initWidget()
{
    m_sizer_main = MainSizer();
    m_sizer_main->SetMinSize(wxSize(FromDIP(350), FromDIP(445)));

    createSwitchTitle();
    createBodyWidget();

    m_sizer_main->AddSpacer(FromDIP(53));
    m_sizer_main->Add(m_page_title_sizer, 0, wxALIGN_CENTER);
    m_sizer_main->Add(m_page_body_sizer, 0, wxALIGN_CENTER);

    Layout();
    Fit();
    //Thaw();
    Centre(wxBOTH);
    Layout();
}

void LoginDialog::initData()
{
   m_panel_checkbox_page1->Refresh();
   m_privacy_policy_page1->Refresh();
   m_service_link_page1->Refresh();
   Layout();
   Refresh();
}

void LoginDialog::initBindEvent()
{
   m_switch_title_1->Bind(wxEVT_LEFT_DOWN, &LoginDialog::title1Clicked, this);
   m_switch_title_2->Bind(wxEVT_LEFT_DOWN, &LoginDialog::title2Clicked, this);
}

void LoginDialog::initOverseaWidget()
{
    m_sizer_main = MainSizer();
    m_sizer_main->SetMinSize(wxSize(FromDIP(380), FromDIP(445)));

    //createBodyWidget();
    m_page_body_sizer = new wxBoxSizer(wxVERTICAL);

    m_page_body_page2_panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* page2Sizer = new wxBoxSizer(wxVERTICAL);
    setupLayoutPage2(page2Sizer,m_page_body_page2_panel,true);
    m_username_ctrl_page2->SetTextHint(2);
    page2Sizer->AddSpacer(FromDIP(63));

    m_page_body_page2_panel->SetSizer(page2Sizer);
    m_page_body_page2_panel->Layout();
    page2Sizer->Fit(m_page_body_page2_panel);

    m_page_body_sizer->Add(m_page_body_page2_panel, 1, wxEXPAND | wxALL, 10);


    m_sizer_main->AddSpacer(FromDIP(50));
    auto oversea_title = new wxStaticText(this,wxID_ANY,_L("Login"));
    oversea_title->SetForegroundColour(wxColour(51,51,51));
    const auto font_title = GetFont().MakeBold();
    oversea_title->SetFont(font_title);
    m_sizer_main->Add(oversea_title,0,wxALIGN_CENTER);
    m_sizer_main->AddSpacer(FromDIP(28));
    m_sizer_main->Add(m_page_body_sizer, 0, wxALIGN_CENTER);

    Layout();
    Fit();
    //Thaw();
    Centre(wxBOTH);
    Layout();
}

void LoginDialog::createBodyWidget()
{
    m_page_body_sizer = new wxBoxSizer(wxVERTICAL);

    // 创建第一个标签页
    m_page_body_page1_panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* page1Sizer = new wxBoxSizer(wxVERTICAL);
    setupLayoutPage1(page1Sizer,m_page_body_page1_panel);
    page1Sizer->AddSpacer(FromDIP(63));

    m_page_body_page1_panel->SetSizer(page1Sizer);
    m_page_body_page1_panel->Layout();
    page1Sizer->Fit(m_page_body_page1_panel);

    // 创建第二个标签页
    m_page_body_page2_panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* page2Sizer = new wxBoxSizer(wxVERTICAL);
    setupLayoutPage2(page2Sizer,m_page_body_page2_panel);
    page2Sizer->AddSpacer(FromDIP(63));

    m_page_body_page2_panel->SetSizer(page2Sizer);
    m_page_body_page2_panel->Layout();
    page2Sizer->Fit(m_page_body_page2_panel);

    // 添加 wxPanel 控件
    m_page_body_sizer->Add(m_page_body_page1_panel, 1, wxEXPAND | wxALL, 10);
    m_page_body_sizer->Add(m_page_body_page2_panel, 1, wxEXPAND | wxALL, 10);
    m_page_body_page2_panel->Hide(); 
    Layout();
    Fit();
}

void LoginDialog::createSwitchTitle()
{
    const auto font_title = GetFont().MakeBold();
    
    //title 1
    m_switch_title_1_sizer = new wxBoxSizer(wxVERTICAL);

    m_switch_title_1_panel = new wxPanel(this, wxID_ANY);

    m_switch_title_1 = new wxStaticText(m_switch_title_1_panel, wxID_ANY, _L("Phone Login/ Register"));
    m_switch_title_1->SetForegroundColour(wxColour(51,51,51));
    m_switch_title_1->SetFont(font_title);

    wxString label_1 = m_switch_title_1->GetLabel();
    wxWindowDC dc_1(m_switch_title_1);
    wxCoord width_1, height_1;
    dc_1.GetTextExtent(label_1, &width_1, &height_1);
    m_title_1_underline = new wxPanel(m_switch_title_1_panel, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(width_1/2.7), FromDIP(4)),wxTAB_TRAVERSAL);
    m_title_1_underline->SetBackgroundColour(wxColour(50,141,251));
    m_title_1_underline->SetForegroundColour(wxColour(50,141,251));

    //auto adjust height
    m_switch_title_1_line_panel = new wxPanel(m_switch_title_1_panel, wxID_ANY);

    m_switch_title_1_sizer->Add(m_switch_title_1, 1, wxALL |wxALIGN_CENTER,0);
    m_switch_title_1_sizer->Add(m_switch_title_1_line_panel, 0, wxEXPAND);
    m_switch_title_1_sizer->Add(m_title_1_underline, 0, wxALL |wxALIGN_CENTER, 0);

    m_switch_title_1_panel->SetSizer(m_switch_title_1_sizer);
    m_switch_title_1_panel->Layout();
    m_switch_title_1_sizer->Fit(m_switch_title_1_panel);

    //title 2
    m_switch_title_2_sizer = new wxBoxSizer(wxVERTICAL);

    m_switch_title_2_panel = new wxPanel(this, wxID_ANY);

    m_switch_title_2 = new wxStaticText(m_switch_title_2_panel, wxID_ANY, _L("Password Login"));
    m_switch_title_2->SetForegroundColour(wxColour(153,153,153));
    m_switch_title_2->SetFont(font_title);
    //auto adjust height
    m_switch_title_2_line_panel = new wxPanel(m_switch_title_2_panel, wxID_ANY);
   
    wxString label_2 = m_switch_title_2->GetLabel();
    wxWindowDC dc_2(m_switch_title_2);
    wxCoord width_2, height_2;
    dc_2.GetTextExtent(label_2, &width_2, &height_2);
    m_title_2_underline = new wxPanel(m_switch_title_2_panel, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(width_2/2.7), FromDIP(4)),wxTAB_TRAVERSAL);
    m_title_2_underline->SetBackgroundColour(wxColour(50,141,251));
    m_title_2_underline->SetForegroundColour(wxColour(50,141,251));
    m_title_2_underline->Hide();

    m_switch_title_2_sizer->Add(m_switch_title_2, 1, wxALL |wxALIGN_CENTER,0);
    m_switch_title_2_sizer->Add(m_switch_title_2_line_panel, 0, wxEXPAND);
    m_switch_title_2_sizer->Add(m_title_2_underline, 0, wxALL |wxALIGN_CENTER, 0);

    m_switch_title_2_panel->SetSizer(m_switch_title_2_sizer);
    m_switch_title_2_panel->Layout();
    m_switch_title_2_sizer->Fit(m_switch_title_2_panel);

    // 创建 wxBoxSizer 布局管理器并添加 wxStaticText 控件
    m_page_title_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_page_title_sizer->Add(m_switch_title_1_panel, 0, wxALIGN_CENTER);
    m_page_title_sizer->AddSpacer(FromDIP(33));
    m_page_title_sizer->Add(m_switch_title_2_panel, 0, wxALIGN_CENTER);
}

void LoginDialog::title1Clicked(wxMouseEvent &event) 
{
    event.Skip(); 
    switchTitle1();
}

void LoginDialog::title2Clicked(wxMouseEvent& event)
{
    event.Skip();
    switchTtitle2();
}

void LoginDialog::switchTitle1() 
{
    m_page_body_page1_panel->Show();
    m_page_body_page2_panel->Hide();
    m_switch_title_1->SetForegroundColour(wxColour(51, 51, 51));
    m_switch_title_2->SetForegroundColour(wxColour(153, 153, 153));
    m_switch_title_1->Refresh();
    m_switch_title_2->Refresh();
    m_title_1_underline->Show();
    m_service_link_page1->Refresh();
    m_privacy_policy_page1->Refresh();

    m_switch_title_1_line_panel->SetMinSize(wxSize(-1, FromDIP(4)));
    m_switch_title_1_sizer->Layout();

    m_title_1_underline->Refresh();
    m_title_2_underline->Hide();

    m_switch_title_2_line_panel->SetMinSize(wxSize(-1, FromDIP(4)));
    m_switch_title_2_sizer->Layout();

    m_title_2_underline->Refresh();
    m_panel_checkbox_page1->Refresh();

    if (m_panel_separotor_login) {
        if (!m_panel_separotor_login->IsShown()) {
            m_panel_separotor_login->Show();
        }
    }
    if (m_panel_separotor_login2) {
        if (m_panel_separotor_login2->IsShown()) {
            m_panel_separotor_login2->Hide();
        }
    }

    m_get_code_button->SetMinSize(wxSize(FromDIP(89), FromDIP(40)));
    // m_get_code_button->SetMaxSize(wxSize(FromDIP(89), FromDIP(40)));
    Layout();
}

void LoginDialog::switchTtitle2() 
{
    m_page_body_page2_panel->Show();
    m_page_body_page1_panel->Hide();
    m_switch_title_2->SetForegroundColour(wxColour(51, 51, 51));
    m_switch_title_1->SetForegroundColour(wxColour(153, 153, 153));
    m_switch_title_1->Refresh();
    m_switch_title_2->Refresh();
    m_page2_checkBox->Refresh();
    m_protocol_page2->Refresh();
    m_service_link_page2->Refresh();
    m_st_and_title2->Refresh();
    m_privacy_policy_page2->Refresh();
    m_title_1_underline->Hide();

    m_switch_title_1_line_panel->SetMinSize(wxSize(-1, FromDIP(4)));
    m_switch_title_1_sizer->Layout();

    m_title_1_underline->Refresh();
    m_title_2_underline->Show();

    m_switch_title_2_line_panel->SetMinSize(wxSize(-1, FromDIP(4)));
    m_switch_title_2_sizer->Layout();

    m_title_2_underline->Refresh();
    m_panel_checkbox_page2->Refresh();
    if (m_panel_separotor_login2) {
        if (!m_panel_separotor_login2->IsShown()) {
            m_panel_separotor_login2->Show();
        }
    }
    if (m_panel_separotor_login) {
        if (m_panel_separotor_login->IsShown()) {
            m_panel_separotor_login->Hide();
        }
    }
    Layout();
    m_password_ctrl_page2->RefreshEyePicPosition();
}
void LoginDialog::setupLayoutPage1(wxBoxSizer* page1Sizer,wxPanel* parent)
{
    wxPanel* panel = new wxPanel(parent, wxID_ANY);

    //left space
    wxPanel* usr_name_space1 = new wxPanel(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER | wxTAB_TRAVERSAL);
    usr_name_space1->SetMinSize(wxSize(FromDIP(53), -1));
    //right space
    wxPanel* usr_name_space2 = new wxPanel(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER | wxTAB_TRAVERSAL);
    usr_name_space2->SetMinSize(wxSize(FromDIP(57), -1));

    m_username_ctrl_page1 = new UserNameCtrl(panel,wxID_ANY,_L("Phone Number / email"));
    m_username_ctrl_page1->SetTextHint(0);
    m_username_ctrl_page1->SetRadius(10);
    m_username_ctrl_page1->Bind(wxEVT_TEXT, &LoginDialog::onUsrNameOrPasswordChangedPage1, this);

    //adjust layout
    wxBoxSizer *last_sizer = new wxBoxSizer(wxHORIZONTAL);
    last_sizer->Add(usr_name_space1, 0, wxEXPAND|wxLeft, 0);
    last_sizer->Add(m_username_ctrl_page1, 0, wxALL, 0);
    last_sizer->Add(usr_name_space2, 0, wxEXPAND ,0);

    // 设置布局
    panel->SetSizer(last_sizer);
    panel->Layout();
    last_sizer->Fit(panel);

    page1Sizer->Add(0,FromDIP(23),0, 0);
    page1Sizer->Add(panel, 0, wxEXPAND ,0);
    page1Sizer->AddSpacer(FromDIP(16));

//*******verify code******** 
    m_verifycode_ctrl_page1 = new VerifyCodeCtrl(parent,wxID_ANY);
    m_verifycode_ctrl_page1->Bind(wxEVT_TEXT, &LoginDialog::onUsrNameOrPasswordChangedPage1, this);

    m_get_code_button = new CountdownButton(parent,_L("Get Code"));
    m_get_code_button->SetMinSize(wxSize(FromDIP(89),FromDIP(40)));
    //m_get_code_button->SetMaxSize(wxSize(FromDIP(89), FromDIP(40)));
    m_get_code_button->Disable();
    m_get_code_button->SetFontDisableColor(wxColour(255, 255, 255));
    m_get_code_button->SetBorderDisableColor(wxColour(221,221,221));
    m_get_code_button->SetBGColor(wxColour(221,221,221));

    m_get_code_button->SetFontHoverColor(wxColour(255, 255, 255));
    m_get_code_button->SetBGHoverColor(wxColour(149,197,255));
    m_get_code_button->SetBorderHoverColor(wxColour(149,197,255));

    m_get_code_button->SetFontPressColor(wxColour(255, 255, 255));
    m_get_code_button->SetBGPressColor(wxColour(17,111,223));
    m_get_code_button->SetBorderPressColor(wxColour(17,111,223));

    m_get_code_button->SetFontColor(wxColour(255, 255, 255));
    m_get_code_button->SetBorderColor(wxColour(50,141,251));
    m_get_code_button->SetBGColor(wxColour(50,141,251));
    m_get_code_button->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& event){
        event.Skip();
        m_get_code_button->SetMinSize(wxSize(FromDIP(89), FromDIP(40)));
        wxString usrname_value = m_username_ctrl_page1->GetValue();
        double num;
        if(usrname_value.ToDouble(&num)){
            //纯数字
            wxRegEx regex(wxT("^1[3456789]\\d{9}$"));
            if (regex.IsValid() && regex.Compile(wxT("^1[3456789]\\d{9}$"),wxRE_ADVANCED)) {
                if(regex.Matches(usrname_value)){
                    ;
                }
                else{
                    page1ShowErrorLabel(_L("Mobile Phone Number Error"));
                    return;
                }
            }
        } else {
            page1ShowErrorLabel(_L("Mobile Phone Number Error"));
            return;
        }

        if(m_get_code_button->GetState()){
            return;
        }
        m_get_code_button->SetState(true);
        m_get_code_button->startTimer();
        //m_get_code_button->Enable(false);
        m_get_code_button->SetEnable(false);

        MultiComUtils::asyncCall(this, [&]() {
            return getSmsCode();
        });  
    });


    //adjust layout
    wxBoxSizer *verify_last_sizer = new wxBoxSizer(wxHORIZONTAL);

    verify_last_sizer->Add(usr_name_space1, 0, wxEXPAND|wxLeft, 0);
    verify_last_sizer->Add(m_verifycode_ctrl_page1);
    verify_last_sizer->AddSpacer(FromDIP(14));
    verify_last_sizer->Add(m_get_code_button);
    verify_last_sizer->Add(usr_name_space2, 0, wxEXPAND ,0);

    page1Sizer->Add(verify_last_sizer, 0, wxEXPAND ,0);

//****error tips ***

    m_error_label = new FFButton(parent, wxID_ANY, _L("Verify code is incorrect"),10);
    m_error_label->Enable(false);
    m_error_label->SetBackgroundColour(*wxWHITE);
    m_error_label->SetBGDisableColor(wxColour("#FACFCA"));
    m_error_label->SetFontDisableColor(wxColour("#EA3522"));
    m_error_label->SetMinSize(wxSize(FromDIP(190), FromDIP(45)));
    m_error_label->Show(false);

    page1Sizer->Add(m_error_label, 0, wxALL | wxCENTER);

    // 添加空白间距
    m_panel_separotor_login = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor_login->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_separotor_login->SetMinSize(wxSize(-1, FromDIP(45)));

    page1Sizer->Add(m_panel_separotor_login);

    //login button
    m_login_button_page1 = new FFButton(parent, wxID_ANY,_L("Login"));
    m_login_button_page1->SetBackgroundColour(*wxWHITE);
    m_login_button_page1->SetFontDisableColor(wxColour(255, 255, 255));
    m_login_button_page1->SetBorderDisableColor(wxColour(221,221,221));
    m_login_button_page1->SetBGColor(wxColour(221,221,221));

    m_login_button_page1->SetFontHoverColor(wxColour(255, 255, 255));
    m_login_button_page1->SetBGHoverColor(wxColour(149,197,255));
    m_login_button_page1->SetBorderHoverColor(wxColour(149,197,255));

    m_login_button_page1->SetFontPressColor(wxColour(255, 255, 255));
    m_login_button_page1->SetBGPressColor(wxColour(17,111,223));
    m_login_button_page1->SetBorderPressColor(wxColour(17,111,223));

    m_login_button_page1->SetFontColor(wxColour(255, 255, 255));
    m_login_button_page1->SetBorderColor(wxColour(50,141,251));
    m_login_button_page1->SetBGColor(wxColour(50,141,251));
    m_login_button_page1->Bind(wxEVT_LEFT_UP,&LoginDialog::onPage1Login, this);
    m_login_button_page1->Disable();
    m_login_button_page1->SetMinSize(wxSize(FromDIP(77),FromDIP(33)));

    page1Sizer->Add(m_login_button_page1, 0, wxALIGN_CENTER_HORIZONTAL);
    page1Sizer->AddSpacer(FromDIP(18));

    //check box
    wxBoxSizer* checkbox_sizer = new wxBoxSizer(wxVERTICAL);
    wxWrapSizer* wrapSizer_1 = new wxWrapSizer(wxHORIZONTAL);
    m_panel_checkbox_page1 = new wxPanel(parent, wxID_ANY,wxDefaultPosition,wxSize(FromDIP(300), -1), wxTAB_TRAVERSAL);

    m_page1_checkBox = new FFCheckBox(m_panel_checkbox_page1, wxID_ANY);
    m_page1_checkBox->SetValue(false);
    m_page1_checkBox->Bind(wxEVT_TOGGLEBUTTON, &LoginDialog::onAgreeCheckBoxChangedPage1, this);

    m_protocol_page1 = new  wxStaticText(m_panel_checkbox_page1, wxID_ANY,_L("Read and Agree to Accept"));
    //m_protocol_page1->SetFont((wxFont(wxFontInfo(14))));

    m_service_link_page1 = new wxStaticText(m_panel_checkbox_page1, wxID_ANY,  _L("《Term of Service》"));
    m_service_link_page1->SetForegroundColour(wxColour(50,141,251));
    m_service_link_page1->Bind(wxEVT_LEFT_DOWN,[this](wxMouseEvent& event){
        event.Skip();
        #if 0
        wxString url = "http://dev.auth.flashforge.shop/en/userAgreement";
        if (m_cur_language.compare("zh_CN") == 0) {
            url = "http://dev.auth.flashforge.shop/userAgreement";
        }
        #else
        wxString url = FFUtils::userAgreement();
        #endif
        wxLaunchDefaultBrowser(url);
    });
    m_service_link_page1->Show(true);

    m_privacy_policy_page1 = new wxStaticText(m_panel_checkbox_page1, wxID_ANY,  _L("《Privacy Policy》"));
    m_privacy_policy_page1->SetForegroundColour(wxColour(50,141,251));
    m_privacy_policy_page1->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e){
        e.Skip();
        #if 0
        wxString url = "http://dev.auth.flashforge.shop/en/privacyPolicy";
        if (m_cur_language.compare("zh_CN") == 0) {
            url = "http://dev.auth.flashforge.shop/privacyPolicy";
        }
        #else
        wxString url = FFUtils::privacyPolicy();
        #endif
        wxLaunchDefaultBrowser(url);
    });
    m_privacy_policy_page1->Show(true);

    m_st_and_title1 = new wxStaticText(m_panel_checkbox_page1, wxID_ANY, _L("and"));

    //left gaption
    wrapSizer_1->Add(m_page1_checkBox);
    wrapSizer_1->AddSpacer(FromDIP(6));
    wrapSizer_1->Add(m_protocol_page1);
    wrapSizer_1->Add(m_service_link_page1);
    wrapSizer_1->Add(m_st_and_title1);
    wrapSizer_1->Add(m_privacy_policy_page1);

    checkbox_sizer->Add(wrapSizer_1, wxSizerFlags(1).Expand());

    m_panel_checkbox_page1->SetSizer(checkbox_sizer);
    m_panel_checkbox_page1->Layout();
    checkbox_sizer->Fit(m_panel_checkbox_page1);

    wxBoxSizer* checkbox_last_sizer = new wxBoxSizer(wxHORIZONTAL);
    checkbox_last_sizer->Add(usr_name_space1, 0, wxEXPAND|wxLeft, 0);
    checkbox_last_sizer->Add(m_panel_checkbox_page1);

    page1Sizer->Add(checkbox_last_sizer, 0, wxEXPAND, 0);
}

void LoginDialog::setupLayoutPage2(wxBoxSizer *page2Sizer, wxPanel *parent, bool foreign)
{
    wxPanel* panel = new wxPanel(parent, wxID_ANY);

    //left space
    wxPanel* usr_name_space1 = new wxPanel(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER | wxTAB_TRAVERSAL);
    usr_name_space1->SetMinSize(wxSize(FromDIP(53), -1));
    //right space
    wxPanel* usr_name_space2 = new wxPanel(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER | wxTAB_TRAVERSAL);
    usr_name_space2->SetMinSize(wxSize(FromDIP(57), -1));

    m_username_ctrl_page2 = new UserNameCtrl(parent,wxID_ANY,_L("Phone Number / email"));
    m_username_ctrl_page2->SetTextHint(1);
    m_username_ctrl_page2->Bind(wxEVT_TEXT, &LoginDialog::onUsrNameOrPasswordChangedPage2, this);

    //adjust layout
    wxBoxSizer *last_sizer = new wxBoxSizer(wxHORIZONTAL);

    last_sizer->Add(usr_name_space1, 0, wxEXPAND|wxLeft, 0);
    last_sizer->Add(m_username_ctrl_page2);
    last_sizer->Add(usr_name_space2, 0, wxEXPAND ,0);

    page2Sizer->Add(0,FromDIP(23),0, 0);
    page2Sizer->Add(last_sizer, 0, wxEXPAND ,0);
    page2Sizer->AddSpacer(FromDIP(16));

//*******password******** 
    m_password_ctrl_page2 = new PasswordCtrl(parent,wxID_ANY);
    m_password_ctrl_page2->Bind(wxEVT_TEXT, &LoginDialog::onUsrNameOrPasswordChangedPage2, this);

    //adjust layout
    wxBoxSizer *verify_last_sizer = new wxBoxSizer(wxHORIZONTAL);
    verify_last_sizer->Add(usr_name_space1, 0, wxEXPAND|wxLeft, 0);
    verify_last_sizer->Add(m_password_ctrl_page2);
    verify_last_sizer->Add(usr_name_space2, 0, wxEXPAND ,0);

    page2Sizer->Add(verify_last_sizer, 0, wxEXPAND ,0);
    page2Sizer->AddSpacer(FromDIP(10));

    //register / forget password
    auto register_link = new wxStaticText(parent, wxID_ANY, _L("Register"));
    register_link->SetForegroundColour(wxColour(50,141,251));
    register_link->Bind(wxEVT_LEFT_DOWN,[this](wxMouseEvent& event){
        event.Skip();
        #if 0
        wxString url = "https://auth.flashforge.com/en/signUp/?channel=Orca";
        if (m_cur_language.compare("zh_CN") == 0) {
            url = "https://auth.flashforge.com/zh/signUp/?channel=Orca";
        }
        #else
        wxString url = FFUtils::userRegister();
        #endif
        wxLaunchDefaultBrowser(url);
    });

    auto forget_password_link = new wxStaticText(parent, wxID_ANY,  _L("Forget Password"));
    forget_password_link->SetForegroundColour(wxColour(50,141,251));
    forget_password_link->Bind(wxEVT_LEFT_DOWN,[this](wxMouseEvent& event){
        event.Skip();
        #if 0
        wxString url = "https://auth.flashforge.com/en/resetPassword/?channel=Orca";
        if (m_cur_language.compare("zh_CN") == 0) {
            url = "https://auth.flashforge.com/zh/resetPassword/?channel=Orca";
        }
        #else
        wxString url = FFUtils::passwordForget();
        #endif
        wxLaunchDefaultBrowser(url);
    });

    wxBoxSizer *regist_forget_hor_sizer = new wxBoxSizer(wxHORIZONTAL);
    regist_forget_hor_sizer->SetMinSize(250,-1);

    regist_forget_hor_sizer->Add(usr_name_space1, 0, wxEXPAND|wxLeft, 0);
    regist_forget_hor_sizer->Add(register_link, 0, wxALIGN_LEFT | wxALIGN_CENTRE_VERTICAL, 0);
    regist_forget_hor_sizer->AddStretchSpacer();
    //regist_forget_hor_sizer->AddSpacer(FromDIP(110));
    regist_forget_hor_sizer->Add(forget_password_link, 0, wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL, 0);
    if (foreign) {
        wxPanel *usr_name_space3 = new wxPanel(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER | wxTAB_TRAVERSAL);
        usr_name_space3->SetMinSize(wxSize(FromDIP(80), -1));
        regist_forget_hor_sizer->Add(usr_name_space3, 0, wxEXPAND, 0);
    } else {
        regist_forget_hor_sizer->Add(usr_name_space2, 0, wxEXPAND, 0);
    }


    page2Sizer->Add(regist_forget_hor_sizer, 0, wxEXPAND,0);

//****error tips ***
    m_error_label_page2 = new FFButton(parent, wxID_ANY, _L("Verify code is incorrect"), 10);
    m_error_label_page2->Enable(false);
    m_error_label_page2->SetBackgroundColour(*wxWHITE);
    m_error_label_page2->SetBGDisableColor(wxColour("#FACFCA"));
    m_error_label_page2->SetFontDisableColor(wxColour("#EA3522"));
    m_error_label_page2->SetMinSize(wxSize(FromDIP(0), FromDIP(0)));
    m_error_label_page2->Show(false);
    page2Sizer->Add(m_error_label_page2, 0, wxALL | wxCENTER);

    // 添加空白间距
    m_panel_separotor_login2 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor_login2->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_separotor_login2->SetMinSize(wxSize(-1, FromDIP(45)));

    page2Sizer->Add(m_panel_separotor_login2);

    //login button
    m_login_button_page2 = new FFButton(parent, wxID_ANY,_L("Login"));
    m_login_button_page2->SetBackgroundColour(*wxWHITE);
    m_login_button_page2->SetFontDisableColor(wxColour(255, 255, 255));
    m_login_button_page2->SetBorderDisableColor(wxColour(221,221,221));
    m_login_button_page2->SetBGDisableColor(wxColour(221,221,221));

    m_login_button_page2->SetFontHoverColor(wxColour(255, 255, 255));
    m_login_button_page2->SetBGHoverColor(wxColour(149,197,255));
    m_login_button_page2->SetBorderHoverColor(wxColour(149,197,255));

    m_login_button_page2->SetFontPressColor(wxColour(255, 255, 255));
    m_login_button_page2->SetBGPressColor(wxColour(17,111,223));
    m_login_button_page2->SetBorderPressColor(wxColour(17,111,223));


    m_login_button_page2->SetFontColor(wxColour(255, 255, 255));
    m_login_button_page2->SetBorderColor(wxColour(50,141,251));
    m_login_button_page2->SetBGColor(wxColour(50,141,251));
    m_login_button_page2->Bind(wxEVT_LEFT_UP,&LoginDialog::onPage2Login, this);
    m_login_button_page2->Disable();
    m_login_button_page2->SetMinSize(wxSize(FromDIP(77),FromDIP(33)));

    page2Sizer->Add(m_login_button_page2, 0, wxALIGN_CENTER_HORIZONTAL);
    page2Sizer->AddSpacer(FromDIP(18));

    //check box
    wxBoxSizer* checkbox_sizer = new wxBoxSizer(wxVERTICAL);
    wxWrapSizer* wrapSizer = new wxWrapSizer(wxHORIZONTAL);
    m_panel_checkbox_page2 = new wxPanel(parent, wxID_ANY,wxDefaultPosition,wxSize(FromDIP(300), -1), wxTAB_TRAVERSAL);

    m_page2_checkBox = new FFCheckBox(m_panel_checkbox_page2, wxID_ANY);
    m_page2_checkBox->SetValue(false);
    m_page2_checkBox->Bind(wxEVT_TOGGLEBUTTON, &LoginDialog::onAgreeCheckBoxChangedPage2, this);

    m_protocol_page2 = new  wxStaticText(m_panel_checkbox_page2, wxID_ANY,_L("Read and Agree to Accept"));
    //m_protocol_page2->SetFont((wxFont(wxFontInfo(14))));
    //Service Item
    m_service_link_page2 = new wxStaticText(m_panel_checkbox_page2, wxID_ANY,  _L("《Term of Service》"));
    m_service_link_page2->SetForegroundColour(wxColour(50,141,251));
    m_service_link_page2->Bind(wxEVT_LEFT_DOWN,[this](wxMouseEvent& event){
        event.Skip();
        #if 0
        wxString url = "http://dev.auth.flashforge.shop/en/userAgreement";
        if (m_cur_language.compare("zh_CN") == 0) {
            url = "http://dev.auth.flashforge.shop/userAgreement";
        }
        #else
        wxString url = FFUtils::userAgreement();
        #endif
        wxLaunchDefaultBrowser(url);
    });

    //privacy Policy
    m_privacy_policy_page2 = new wxStaticText(m_panel_checkbox_page2, wxID_ANY,  _L("《Privacy Policy》"));
    m_privacy_policy_page2->SetForegroundColour(wxColour(50,141,251));
    m_privacy_policy_page2->Bind(wxEVT_LEFT_DOWN,[this](wxMouseEvent& event){
        event.Skip();
        #if 0
        wxString url = "http://dev.auth.flashforge.shop/en/privacyPolicy";
        if (m_cur_language.compare("zh_CN") == 0) {
            url = "http://dev.auth.flashforge.shop/privacyPolicy";
        }
        #else
        wxString url = FFUtils::privacyPolicy();
        #endif
        wxLaunchDefaultBrowser(url);
    });

    m_st_and_title2 = new wxStaticText(m_panel_checkbox_page2, wxID_ANY, _L("and"));

    //left gaption
    wrapSizer->Add(m_page2_checkBox);
    wrapSizer->AddSpacer(FromDIP(6));
    wrapSizer->Add(m_protocol_page2);
    /*wrapSizer->Add(and_txt);
    wrapSizer->Add(agree_txt);
    wrapSizer->Add(to_txt);
    wrapSizer->Add(accept_txt);*/
    wrapSizer->Add(m_service_link_page2);
    wrapSizer->Add(m_st_and_title2);
    wrapSizer->Add(m_privacy_policy_page2);

    checkbox_sizer->Add(wrapSizer, wxSizerFlags(1).Expand());

    m_panel_checkbox_page2->SetSizer(checkbox_sizer);
    m_panel_checkbox_page2->Layout();
    checkbox_sizer->Fit(m_panel_checkbox_page2);

    wxBoxSizer* checkbox_last_sizer = new wxBoxSizer(wxHORIZONTAL);
    checkbox_last_sizer->Add(usr_name_space1, 0, wxEXPAND|wxLeft, 0);
    checkbox_last_sizer->Add(m_panel_checkbox_page2);
    checkbox_last_sizer->Add(usr_name_space2, 0, wxEXPAND ,0);

    page2Sizer->Add(checkbox_last_sizer, 0, wxEXPAND, 0);
}

void LoginDialog::onUsrNameOrPasswordChangedPage1(wxCommandEvent& event)
{
    //
        event.Skip();
        wxString username = m_username_ctrl_page1->GetValue();
        wxString verifycode = m_verifycode_ctrl_page1->GetValue();
        bool agree = m_page1_checkBox->GetValue();
        if (username.IsEmpty() || m_get_code_button->GetState()) {
            m_get_code_button->Disable();
            m_get_code_button->Refresh();
        } else {
            m_get_code_button->Enable();
            m_get_code_button->Refresh();
        }
        if (!username.IsEmpty() && !verifycode.IsEmpty() && agree)
        {
            m_login_button_page1->Enable();
            m_login_button_page1->Refresh();
        }
        else{
            m_login_button_page1->Disable();
            m_login_button_page1->Refresh(); 
        }
}

void LoginDialog::onAgreeCheckBoxChangedPage1(wxCommandEvent& event)
{
        event.Skip();
        wxString username = m_username_ctrl_page1->GetValue();
        wxString verifycode = m_verifycode_ctrl_page1->GetValue();
        bool agree = m_page1_checkBox->GetValue();
        m_page1_checkBox->SetValue(agree);
        m_get_code_button->SetMinSize(wxSize(FromDIP(89), FromDIP(40)));
        if (!username.IsEmpty() && !verifycode.IsEmpty() && agree)
        {
            m_login_button_page1->Enable();
            m_login_button_page1->Refresh();
        }
        else{
            m_login_button_page1->Disable();
            m_login_button_page1->Refresh();
        }
}

void LoginDialog::onUsrNameOrPasswordChangedPage2(wxCommandEvent& event)
{
        event.Skip();
        wxString username = m_username_ctrl_page2->GetValue();
        wxString password = m_password_ctrl_page2->GetValue();
        bool agree = m_page2_checkBox->GetValue();
        if (!username.IsEmpty() && !password.IsEmpty() && agree)
        {
            m_login_button_page2->Enable();
            m_login_button_page2->Refresh();
        }
        else{
            m_login_button_page2->Disable();
            m_login_button_page2->Refresh();
        }
}

void LoginDialog::onAgreeCheckBoxChangedPage2(wxCommandEvent& event)
{
        event.Skip();
        wxString username = m_username_ctrl_page2->GetValue();
        wxString password = m_password_ctrl_page2->GetValue();
        bool agree = m_page2_checkBox->GetValue();
        m_page2_checkBox->SetValue(agree);
        if (!username.IsEmpty() && !password.IsEmpty() && agree)
        {
            m_login_button_page2->Enable();
            m_login_button_page2->Refresh();
        }
        else{
            m_login_button_page2->Disable();
            m_login_button_page2->Refresh();
        }
}

void LoginDialog::onPage1Login(wxMouseEvent& event)
{
    event.Skip();
    if(m_login1_pressed){
        return;
    }
    m_login1_pressed = true;
//    m_login_button_page1->Enable(false);
    m_get_code_button->SetMinSize(wxSize(FromDIP(89), FromDIP(40)));
    wxString usrname = m_username_ctrl_page1->GetValue();
    wxString usrname_value = m_username_ctrl_page1->GetValue();
    double   num;
    if (usrname_value.ToDouble(&num)) {
        // 纯数字
        wxRegEx regex(wxT("^1[3456789]\\d{9}$"));
        if (regex.IsValid() && regex.Compile(wxT("^1[3456789]\\d{9}$"), wxRE_ADVANCED)) {
          if (regex.Matches(usrname_value)) {
                ;
          } else {
             page1ShowErrorLabel(_L("Mobile Phone Number Error"));
              return;
          }
        }
    } else {
            page1ShowErrorLabel(_L("Mobile Phone Number Error"));
            return;
    }
    wxString verify_code = m_verifycode_ctrl_page1->GetValue();
    com_token_data_t token_data;
    std::string message;
    std::string language = serverLanguageEn;
    if (m_cur_language.compare("zh_CN") == 0) {
        language = serverLanguageZh;
    }
    ComErrno login_result = MultiComUtils::getTokenBySMSCode(usrname.ToStdString(), verify_code.ToStdString(), language, token_data,message);
    if(login_result == ComErrno::COM_OK){
        ComErrno add_dev_result = MultiComMgr::inst()->addWanDev(token_data, 3, 200);
        if (add_dev_result == COM_OK) {
             m_usr_name = usrname.ToStdString();
             LoginDialog::m_token_data = token_data;
             wxGetApp().handle_login_result("default.jpg", usrname.ToStdString());
             BOOST_LOG_TRIVIAL(info) << "usr login succeed 111 : LoginDialog::onPage1Login";
            m_login1_pressed = true;
#ifdef _WIN32
             //Hide();
             Close();
#else if __APPLE__
             Close();
#endif
             AppConfig *app_config = wxGetApp().app_config;
             if (app_config) {
                // 主动点击登录，设置token值
                app_config->set("usr_input_name", usrname.ToStdString());
                app_config->set("access_token", token_data.accessToken);
                app_config->set("refresh_token", token_data.refreshToken);
                app_config->set("token_expire_time", std::to_string(token_data.expiresIn));
                app_config->set("token_start_time", std::to_string(token_data.startTime));
             } 
        } else {
             page1ShowErrorLabel(_L("Server connection exception"));
             BOOST_LOG_TRIVIAL(error) << "Server connection exception : addWanDev interface failed !";
             flush_logs();
        }
    } else {
        if (!message.empty()) {
             page1ShowErrorLabel(wxString::FromUTF8(message));
        } else {
             if (login_result == ComErrno::COM_INVALID_VALIDATION)
             {
                page1ShowErrorLabel(_L("Verify code is incorrect"));
             }
             else if (login_result == ComErrno::COM_ERROR)
             {
                page1ShowErrorLabel(_L("Server connection exception"));
                BOOST_LOG_TRIVIAL(error) << "Server connection exception :ComErrno::COM_ERROR ";
                flush_logs();
             }
             else if (login_result == ComErrno::COM_UNREGISTER_USER)
             {
                page1ShowErrorLabel(_L("User not registered"));
             }
        }
    }
}

void LoginDialog::page1ShowErrorLabel(const wxString& labelInfo)
{
    m_timer.Bind(wxEVT_TIMER, &LoginDialog::OnTimer, this);
    m_error_label->SetLabel(labelInfo);

    wxGCDC dc(this);
    int sw = dc.GetTextExtent(labelInfo).x;
    if (sw >= 380) {
        sw = 380;
    } else if (sw < 340) {
        sw += 40;
    }
    if (sw >= 380) {
        sw = 380;
    }
    m_error_label->SetMinSize(wxSize(FromDIP(sw), FromDIP(45)));

    m_panel_separotor_login->Hide();
    m_error_label->Show(true);
    Layout();
    startTimer();
}

void LoginDialog::onPage2Login(wxMouseEvent& event)
{
    event.Skip();
//    m_login_button_page2->Enable(false);
    if(m_login2_pressed){
        return;
    }
    m_login2_pressed = true;
    wxString usrname = m_username_ctrl_page2->GetValue();
    wxString password = m_password_ctrl_page2->GetValue();
    double num;
    if(usrname.ToDouble(&num)){
        //纯数字
        wxRegEx regex(wxT("^1[3456789]\\d{9}$"));
        if (regex.IsValid() && regex.Compile(wxT("^1[3456789]\\d{9}$"),wxRE_ADVANCED)) {
            if(regex.Matches(usrname)){
                ;
            }
            else{
                page2ShowErrorLabel(_L("Mobile Phone Number Error"));
                event.Skip();
                return;
            }
        }
    }
    else{
        //邮箱
        wxRegEx regex("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
        if (regex.IsValid() && regex.Compile("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$",wxRE_ADVANCED)) {
            if(regex.Matches(usrname)){
                ;
            }
            else{
                page2ShowErrorLabel(_L("Email Address Error"));
                event.Skip();
                return;
            }
        }
    }

    // 1、getTokenByPassword   2、addWanDev
    com_token_data_t token_data;
    std::string message;
    std::string language = serverLanguageEn;
    if (m_cur_language.compare("zh_CN") == 0) {
        language = serverLanguageZh;
    }
    const char *charData = password.mb_str(wxConvUTF8);
    std::string finalPassword(charData);
    ComErrno    login_result = MultiComUtils::getTokenByPassword(usrname.ToStdString(), finalPassword, language, token_data, message);
    if (login_result == ComErrno::COM_OK) {
        ComErrno add_dev_result = MultiComMgr::inst()->addWanDev(token_data, 3, 200);
        if (add_dev_result == COM_OK) {
            m_usr_name = usrname.ToStdString();
            LoginDialog::m_token_data = token_data;
            wxGetApp().handle_login_result("default.jpg", usrname.ToStdString());
            BOOST_LOG_TRIVIAL(info) << "usr login succeed 222 : LoginDialog::onPage2Login";
            m_login2_pressed = false;
#ifdef _WIN32
            //Hide();
            Close();
            BOOST_LOG_TRIVIAL(info) << "usr login succeed , hide window!";
#else if __APPLE__
            Close();
#endif
            AppConfig *app_config = wxGetApp().app_config;
            if (app_config) {
                // 主动点击登录，设置token值
                app_config->set("usr_input_name", usrname.ToStdString());
                app_config->set("access_token", token_data.accessToken);
                app_config->set("refresh_token", token_data.refreshToken);
                app_config->set("token_expire_time", std::to_string(token_data.expiresIn));
                app_config->set("token_start_time", std::to_string(token_data.startTime));
            }
        } else {
            page2ShowErrorLabel(_L("Server connection exception"));
            BOOST_LOG_TRIVIAL(error) << "Server connection exception : addWanDev interface failed !";
            flush_logs();
        }
    } else {
        if (!message.empty()) {
            page2ShowErrorLabel(wxString::FromUTF8(message));
        } else {
            if (login_result == ComErrno::COM_INVALID_VALIDATION)
            {
                page2ShowErrorLabel(_L("Password is incorrect"));
            }
            else if (login_result == ComErrno::COM_ERROR)
            {
                page2ShowErrorLabel(_L("Server connection exception"));
                BOOST_LOG_TRIVIAL(error) << "Server connection exception : ComErrno::COM_ERROR !";
                flush_logs();
            }
            else if (login_result == ComErrno::COM_UNREGISTER_USER)
            {
                page2ShowErrorLabel(_L("User not registered"));
            }
        }
    }
}

void LoginDialog::page2ShowErrorLabel(const wxString& labelInfo)
{
    m_timer.Bind(wxEVT_TIMER, &LoginDialog::OnTimer, this);
    m_error_label_page2->SetLabel(labelInfo);
    
    wxGCDC dc(this);
    int sw = dc.GetTextExtent(labelInfo).x;
    if (sw >= 380) {
        sw = 380;
    } else if (sw < 340) {
        sw += 40;
    }
    if (sw >= 380) {
        sw = 380;
    }

    m_error_label_page2->SetMinSize(wxSize(FromDIP(sw), FromDIP(45)));
    m_panel_separotor_login2->Hide();
    m_error_label_page2->Show(true);
    Layout();
    startTimer();
}

void LoginDialog::OnTimer(wxTimerEvent& event)
{
    event.Skip();
    if (m_panel_separotor_login) {
        if (!m_panel_separotor_login->IsShown()) {
            m_panel_separotor_login->Show();
        }
    }
    if (m_panel_separotor_login2) {
        if (!m_panel_separotor_login2->IsShown()) {
            m_panel_separotor_login2->Show();
        }
    }

    if (m_error_label_panel) {
        if (m_error_label_panel->IsShown()) {
            m_error_label_panel->Show(false);
        }
    }

    if (m_error_label_page2_panel) {
        if (m_error_label_page2_panel->IsShown()) {
            m_error_label_page2_panel->Show(false);
        }
    }

    if (m_error_label) {
        if (m_error_label->IsShown()) {
            m_error_label->Show(false);
            m_login_button_page1->Enable(true);
            m_login1_pressed = false;
        }
    }

    if (m_error_label_page2) {
        if (m_error_label_page2->IsShown()) {
            m_error_label_page2->Show(false);
            m_login_button_page2->Enable(true);
            m_login2_pressed = false;
        }
    }

    m_timer.Stop();
}

ComErrno LoginDialog::getSmsCode()
{
    if (m_first_call_client_token) {
        ComErrno get_result = MultiComUtils::getClientToken(m_client_SMS_token);
        if (get_result == ComErrno::COM_ERROR) {
            page1ShowErrorLabel(_L("Server connection exception"));
            BOOST_LOG_TRIVIAL(warning) << boost::format("MultiComUtils::getClientToken Failed!");
            return COM_ERROR;
        } else if (get_result == ComErrno::COM_OK) {
            m_first_call_client_token = false;
        }
    }
    // std::string message;
    ComErrno send_result = MultiComUtils::sendSMSCode(m_client_SMS_token.accessToken, m_username_ctrl_page1->GetValue().ToStdString(), "en",m_sms_info);
    if (send_result == ComErrno::COM_ERROR) {
        BOOST_LOG_TRIVIAL(warning) << boost::format("MultiComUtils::sendSMSCode Failed!");
        BOOST_LOG_TRIVIAL(error) << m_sms_info;
        return COM_ERROR;
    }
    if (send_result == COM_OK) {
        return COM_OK;
    }
    return COM_ERROR;
}

}
}
