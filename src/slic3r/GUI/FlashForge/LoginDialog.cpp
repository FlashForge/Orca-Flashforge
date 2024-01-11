#include "LoginDialog.hpp"

#include "slic3r/GUI/I18N.hpp"

#include "libslic3r/Utils.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include "slic3r/GUI/format.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "MultiComUtils.hpp"
#include "MultiComMgr.hpp"

#include <wx/clipbrd.h>
#include <wx/dcgraph.h>

#include <regex>

namespace Slic3r {
namespace GUI {

    wxDEFINE_EVENT(EVT_UPDATE_TEXT_LOGIN, wxCommandEvent);

    com_token_data_t LoginDialog::m_token_data = {};

    CountdownButton::CountdownButton(wxWindow* parent, wxString text, wxString icon /*= ""*/, long style /*= 0*/, int iconSize /*= 0*/, wxWindowID btn_id /*= wxID_ANY*/)
        : Button(parent,text,icon,style,iconSize,btn_id)
        , m_countdown(60)
        , m_parent(parent)
    {
        // 创建一个 wxTimer 对象，并将其绑定到当前控件上
        m_timer.Bind(wxEVT_TIMER, &CountdownButton::OnTimer, this);
        //Bind(EVT_UPDATE_TEXT_LOGIN, &CountdownButton::OnUpdateText, this);
    }

    void CountdownButton::OnTimer(wxTimerEvent& event)
    {
        // 更新倒计时
        m_countdown--;
        if (m_countdown > 0)
        {
            // 更新按钮上的文本
            SetLabel(wxString::Format("%d s", m_countdown));
        }
        else
        {
            // 停止定时器
            m_timer.Stop();
            m_countdown = 60;

            // 恢复按钮上原来的文本
            SetLabel(_L("Get Code"));
        }
    }

    VerifycodeTextCtrl::VerifycodeTextCtrl(wxBitmap verifycodebitmap,wxWindow *parent, wxWindowID id/* = wxID_ANY*/)
                       : wxPanel(parent, id)
    {
        // 创建验证码输入框和锁图标
        m_verify_staticbitmap = new wxStaticBitmap(this, wxID_ANY,verifycodebitmap);
        m_verify_code_text_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_LEFT | wxBORDER_NONE);
        m_verify_code_text_ctrl->SetHint(_L("Code"));
        m_verify_code_text_ctrl->SetMinSize(wxSize(171,33));

        // 创建垂直布局并添加控件
        wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);
        hbox->Add(m_verify_staticbitmap,wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 5));
        hbox->Add(m_verify_code_text_ctrl, wxSizerFlags().Expand().Border(wxTOP, 12));

        // 设置面板的布局
        SetSizerAndFit(hbox);
    }

    //增加正则过滤
    // 邮箱正则表达式
    const std::regex emailRegex(R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Z|a-z]{2,}\b)");

    // 手机号码正则表达式
    const std::regex phoneRegex(R"(\b1[3456789]\d{9}\b)");

    // 邮箱和手机号码过滤器
    class EmailPhoneValidator : public wxTextValidator
    {
    public:
        EmailPhoneValidator() : wxTextValidator(wxFILTER_INCLUDE_CHAR_LIST) {
            std::regex emailRegex(R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b)");
            std::regex phoneRegex(R"(\b1[3456789]\d{9}\b)");

            m_regexps.push_back(emailRegex);
            m_regexps.push_back(phoneRegex);
        }

        wxString IsValid(const wxString& value) const override
        {
            for (const auto& regex : m_regexps)
            {
                if (std::regex_match(value.ToStdString(), regex))
                {
                    return value;
                }
            }
            return wxEmptyString;
        }
    private:
        std::vector<std::regex> m_regexps;
    };

    UsrnameTextCtrl::UsrnameTextCtrl(wxBitmap usrnamebitmap,wxWindow *parent, wxWindowID id /*= wxID_ANY*/)
                    : wxPanel(parent, id)
    {
        // 创建账号输入框和人像图标
        m_usr_staticbitmap = new wxStaticBitmap(this, wxID_ANY,usrnamebitmap);
        m_user_name_text_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_LEFT | wxBORDER_NONE);
        m_user_name_text_ctrl->SetHint(_L("Phone Number / email"));
        m_user_name_text_ctrl->SetMinSize(wxSize(295,33));
        // wxTextValidator validator(wxFILTER_INCLUDE_CHAR_LIST);
        // validator.SetCharIncludes("^[\w\.-]+@[\w\.-]+\.\w+$|^1[3-9]\d{9}$");
        // m_user_name_text_ctrl->SetValidator(validator);

        // 创建垂直布局并添加控件
        wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);
        hbox->Add(m_usr_staticbitmap,wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 5));
        hbox->Add(m_user_name_text_ctrl, wxSizerFlags().Expand().Border(wxTOP, 12));

        // 设置面板的布局
        SetSizerAndFit(hbox);
    }

    PasswordTextCtrl::PasswordTextCtrl(wxBitmap lockbitmap,wxBitmap eyeoffbitmapBtn,wxBitmap eyeonbitmapBtn,wxWindow *parent, wxWindowID id /*= wxID_ANY*/)
                    : wxPanel(parent, id)
    {
        m_eye_off_bitmap = eyeoffbitmapBtn;
        m_eye_on_bitmap = eyeonbitmapBtn;

        // 创建密码输入框和眼睛图标按钮
        m_lock_staticbitmap = new wxStaticBitmap(this, wxID_ANY,lockbitmap);
        m_password_text_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD | wxTE_PROCESS_ENTER | wxTE_LEFT | wxBORDER_NONE);
        m_password_text_ctrl->SetHint(_L("Password"));
        m_password_text_ctrl->SetMinSize(wxSize(241,33));

        m_showPassword_staticbitmap = new wxStaticBitmap(this, wxID_ANY,eyeoffbitmapBtn);

        // 设置眼睛图标按钮的大小和工具提示文本
        m_showPassword_staticbitmap->SetSize(wxSize(20, 16));

        // 创建垂直布局并添加控件
        wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);
        hbox->Add(m_lock_staticbitmap,wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 5));
        hbox->Add(m_password_text_ctrl, wxSizerFlags().Expand().Border(wxTOP, 12));
        hbox->Add(m_showPassword_staticbitmap, wxSizerFlags().Center().Border(wxLEFT | wxRIGHT, 5));

        // 设置面板的布局
        SetSizerAndFit(hbox);

        // 连接信号和槽函数
        m_showPassword_staticbitmap->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e){OnShowPasswordButtonClicked(e);});

		m_plain_text_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER | wxTE_LEFT | wxBORDER_NONE);
        m_plain_text_ctrl->Hide();
    }

    wxString PasswordTextCtrl::GetValue(){
        if(m_encrypt){
            return m_password_text_ctrl->GetValue();
        }
        else{
            return m_plain_text_ctrl->GetValue();
        }
    }

    void PasswordTextCtrl::RefreshEyePicPosition()
    {
        if(!m_encrypt){
            //明文显示时调整眼睛图标位置
            //m_lock_staticbitmap->SetPosition(m_eye_pic_position);
            m_showPassword_staticbitmap->SetPosition(m_eye_pic_position);
            m_showPassword_staticbitmap->Refresh();
            m_showPassword_staticbitmap->Update();
        }
    }

    void PasswordTextCtrl::OnShowPasswordButtonClicked(wxMouseEvent& event)
    {
        if (m_encrypt) {
            m_encrypt = false;
            wxString password = m_password_text_ctrl->GetValue();
            //获取眼睛图标位置(明文显示时，因为密文控件被隐藏，会导致图标显示位置变更)
            m_eye_pic_position = m_showPassword_staticbitmap->GetPosition();
            // 如果密码输入框当前为密码模式，则切换为明文模式
            m_password_text_ctrl->SetWindowStyle(wxTE_PROCESS_ENTER | wxTE_LEFT | wxBORDER_NONE);
            m_password_text_ctrl->SetValue(password);
            m_showPassword_staticbitmap->SetBitmap(m_eye_on_bitmap);
            m_password_text_ctrl->Refresh();
            m_password_text_ctrl->Update();

        //替换为明文控件
            m_password_text_ctrl->Hide();
		    wxPoint point = m_password_text_ctrl->GetPosition();
            wxSize size = m_password_text_ctrl->GetSize();
            m_plain_text_ctrl->SetPosition(point);
            m_plain_text_ctrl->SetSize(size);
            m_plain_text_ctrl->SetValue(wxEmptyString);
            m_plain_text_ctrl->SetValue(password);
            m_plain_text_ctrl->Show();
        } else {
            m_encrypt = true;
            m_password_text_ctrl->Refresh();
            m_password_text_ctrl->SetWindowStyle(wxTE_PASSWORD | wxTE_LEFT | wxBORDER_NONE);
            //MarkedText();
            // 如果密码输入框当前为明文模式，则切换为密码模式
            m_password_text_ctrl->SetValue(m_password_text_ctrl->GetValue());
            m_showPassword_staticbitmap->SetBitmap(m_eye_off_bitmap);
            m_password_text_ctrl->Refresh();
            m_password_text_ctrl->Update();

        //替换为密文控件
            wxString plain_text = m_plain_text_ctrl->GetValue();
            m_plain_text_ctrl->Hide();
            m_password_text_ctrl->SetValue(wxEmptyString);
            m_password_text_ctrl->SetValue(plain_text);
            m_password_text_ctrl->Show();

        }
    }

LoginDialog::LoginDialog()
    :TitleDialog(static_cast<wxWindow *>(wxGetApp().mainframe),_L("Login"),6)
{
    SetFont(wxGetApp().normal_font());
	SetBackgroundColour(*wxWHITE);

    //std::string icon_path = (boost::format("%1%/images/Orca-FlashforgeTitle.ico") % resources_dir()).str();
    //SetIcon(wxIcon(encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));
 
    initWidget();

    initBindEvent();
    initData();

}

com_token_data_t LoginDialog::GetLoginToken()
{
    return m_token_data;
}

void LoginDialog::SetToken(std::string accessToken, std::string refreshToken)
{
    m_token_data.accessToken = accessToken;
    m_token_data.refreshToken = refreshToken;
}

void LoginDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    const int& em = em_unit();
    const wxSize& size = wxSize(65 * em, 500 * em);
    SetMinSize(size);
    Fit();
    Refresh();
}

void LoginDialog::OnPaint(wxPaintEvent& event)
{
    return;
    wxPaintDC dc(this);

    // 获取对话框的客户区域
    wxRect rect = GetClientRect();

    // 绘制标题背景色
    wxColour titleBgColor(0xE1, 0xE2, 0xE6);
    dc.SetBrush(wxBrush(titleBgColor));
    dc.SetPen(wxPen(titleBgColor));
    dc.DrawRectangle(rect.x, rect.y, rect.width, FromDIP(29));

    // 绘制正文背景色
    wxColour bodyBgColor(0xFF, 0xFF, 0xFF);
    dc.SetBrush(wxBrush(bodyBgColor));
    dc.SetPen(wxPen(bodyBgColor));
    dc.DrawRectangle(rect.x, rect.y + FromDIP(29), rect.width, rect.height - FromDIP(29));

    // 绘制标题
    wxFont font(wxFontInfo(18).Bold());
    wxString title = GetTitle();
    wxSize titleSize = dc.GetTextExtent(title);
    int x = rect.x + (rect.width - titleSize.GetWidth()) / 2;
    int y = rect.y + (FromDIP(29) - titleSize.GetHeight()) / 2;
    dc.SetFont(font);
    dc.SetTextForeground(wxColour(0, 0, 0));
    dc.DrawText(title, x, y);
}

void LoginDialog::initWidget()
{
    m_sizer_main = MainSizer();
    m_sizer_main->SetMinSize(wxSize(0, -1));

    createSwitchTitle();
    createBodyWidget();

    m_sizer_main->AddSpacer(FromDIP(53));
    m_sizer_main->Add(m_page_title_sizer, 0, wxALIGN_CENTER);
    m_sizer_main->Add(m_page_body_sizer, 0, wxALIGN_CENTER);

    Layout();
    Fit();
    Thaw();
    Centre(wxBOTH);
    Layout();
}

void LoginDialog::initData()
{
    ComErrno get_result = MultiComUtils::getClientToken(m_client_SMS_token);
   if(get_result == ComErrno::COM_ERROR){
        BOOST_LOG_TRIVIAL(warning) << boost::format("MultiComUtils::getClientToken Failed!");
   }
}

void LoginDialog::initBindEvent()
{
    m_switch_title_1->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e){
        m_page_body_page1_panel->Show();
        m_page_body_page2_panel->Hide();
        m_switch_title_1->SetForegroundColour(wxColour(51,51,51));
        m_switch_title_2->SetForegroundColour(wxColour(153,153,153));
        m_switch_title_1->Refresh();
        m_switch_title_2->Refresh();
        m_title_1_underline->Show();
        //m_staticLine_verify->SetForegroundColour(wxColour(51,51,51));

        m_switch_title_1_line_panel->SetMinSize(wxSize(-1,FromDIP(4)));
        m_switch_title_1_sizer->Layout();

        m_title_1_underline->Refresh();
        m_title_2_underline->Hide();

        m_switch_title_2_line_panel->SetMinSize(wxSize(-1, FromDIP(4)));
        m_switch_title_2_sizer->Layout();

        m_title_2_underline->Refresh();
        Layout();
        });
    m_switch_title_2->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e){
        m_page_body_page2_panel->Show();
        m_page_body_page1_panel->Hide();
        m_switch_title_2->SetForegroundColour(wxColour(51,51,51));
        m_switch_title_1->SetForegroundColour(wxColour(153,153,153));
        m_switch_title_1->Refresh();
        m_switch_title_2->Refresh();
        m_page2_checkBox->Refresh();
        m_protocol_page2->Refresh();
        m_service_link_page2->Refresh();
        m_privacy_policy_page2->Refresh();
        m_title_1_underline->Hide();

        m_switch_title_1_line_panel->SetMinSize(wxSize(-1,  FromDIP(4)));
        m_switch_title_1_sizer->Layout();

        m_title_1_underline->Refresh();
        m_title_2_underline->Show();

        m_switch_title_2_line_panel->SetMinSize(wxSize(-1,  FromDIP(4)));
        m_switch_title_2_sizer->Layout();

        m_title_2_underline->Refresh();
        m_panel_checkbox_page2->Refresh();
        Layout();
        m_password->RefreshEyePicPosition();
        });
    
    //Bind(wxEVT_PAINT, &LoginDialog::OnPaint, this);
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

    m_switch_title_1 = new wxStaticText(m_switch_title_1_panel, wxID_ANY, _L("Verify Code Login/ Register"));
    m_switch_title_1->SetForegroundColour(wxColour(51,51,51));
    m_switch_title_1->SetFont(font_title);

    m_title_1_underline = new wxPanel(m_switch_title_1_panel, wxID_ANY, wxDefaultPosition, wxSize(90, 4),wxTAB_TRAVERSAL);
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
   
    m_title_2_underline = new wxPanel(m_switch_title_2_panel, wxID_ANY, wxDefaultPosition, wxSize(90, 4),wxTAB_TRAVERSAL);
    m_title_2_underline->SetBackgroundColour(wxColour(50,141,251));
    m_title_2_underline->SetForegroundColour(wxColour(50,141,251));
    m_title_2_underline->Hide();

    // m_staticLine_password->Hide();
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

void LoginDialog::setupLayoutPage1(wxBoxSizer* page1Sizer,wxPanel* parent)
{
    wxPanel* panel = new wxPanel(parent, wxID_ANY);

    //phone number / email
    wxBitmap usr_pic = create_scaled_bitmap("login-usr", this, 18);

    //left space
    wxPanel* usr_name_space1 = new wxPanel(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER | wxTAB_TRAVERSAL);
    usr_name_space1->SetMinSize(wxSize(66, -1));
    //right space
    wxPanel* usr_name_space2 = new wxPanel(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER | wxTAB_TRAVERSAL);
    usr_name_space2->SetMinSize(wxSize(86, -1));

    m_usrname_page1 = new UsrnameTextCtrl(usr_pic,parent,wxID_ANY);
    m_usrname_page1->Bind(wxEVT_TEXT, &LoginDialog::onUsrNameOrPasswordChangedPage1, this);
    wxStaticBoxSizer* border_sizer = new wxStaticBoxSizer(wxVERTICAL, parent, wxEmptyString);
    border_sizer->Add(m_usrname_page1);
    //add border
    border_sizer->GetStaticBox()->SetSizeHints(-1, -1, -1, 100);

    //adjust layout
    wxBoxSizer *last_sizer = new wxBoxSizer(wxHORIZONTAL);
    last_sizer->Add(usr_name_space1, 0, wxEXPAND|wxLeft, 0);
    last_sizer->Add(border_sizer, 0, wxALL, 0);
    last_sizer->Add(usr_name_space2, 0, wxEXPAND ,0);

    page1Sizer->Add(0,FromDIP(23),0, 0);
    page1Sizer->Add(last_sizer, 0, wxEXPAND ,0);

//*******verify code******** 

    // bitmap
    wxBitmap verify_pic = create_scaled_bitmap("login-verify", this, 18);

    wxStaticBoxSizer* verify_border_sizer = new wxStaticBoxSizer(wxVERTICAL, parent, wxEmptyString);
    verify_border_sizer->SetMinSize(wxSize(190,45));

    m_verify_code = new VerifycodeTextCtrl(verify_pic,parent,wxID_ANY);
    m_verify_code->Bind(wxEVT_TEXT, &LoginDialog::onUsrNameOrPasswordChangedPage1, this);
    verify_border_sizer->Add(m_verify_code);
    //add border
    verify_border_sizer->GetStaticBox()->SetSizeHints(-1, -1, -1, 100);

    m_get_code_button = new CountdownButton(parent,_L("Get Code"));
    m_get_code_button->SetForegroundColour(wxColour(255, 255, 255));// 设置字体颜色
    m_get_code_button->SetBackgroundColour(wxColour(221,221,221)); // 设置背景色
    m_get_code_button->SetWindowStyleFlag(wxBORDER_NONE); //去除边框线
    m_get_code_button->SetMinSize(wxSize(114,54));
    m_get_code_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent& event){
        //发送验证码
        ComErrno send_result =MultiComUtils::sendSMSCode(m_client_SMS_token.accessToken,m_usrname_page1->GetValue().ToStdString());
        if(send_result == ComErrno::COM_ERROR){
            BOOST_LOG_TRIVIAL(warning) << boost::format("MultiComUtils::sendSMSCode Failed!");
        }
        m_get_code_button->startTimer();
    });
    //Bind(EVT_UPDATE_TEXT_LOGIN, &LoginDialog::OnUpdateText, this);


    //adjust layout
    wxBoxSizer *verify_last_sizer = new wxBoxSizer(wxHORIZONTAL);
    //verify_last_sizer->SetMinSize(250,-1);

    verify_last_sizer->Add(usr_name_space1, 0, wxEXPAND|wxLeft, 0);
    verify_last_sizer->Add(verify_border_sizer);
    verify_last_sizer->Add(m_get_code_button,0, wxALL,10);
    verify_last_sizer->Add(usr_name_space2, 0, wxEXPAND ,0);

    page1Sizer->Add(verify_last_sizer, 0, wxEXPAND ,0);

//****error tips ***
    m_error_label = new wxStaticText(parent,wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize);
    m_error_label->SetLabel(_L("Verify code is incorrect"));
    m_error_label->SetFont((wxFont(wxFontInfo(16))));
    m_error_label->SetBackgroundColour(wxColour(250, 207, 202)); // #FACFCA
    m_error_label->SetForegroundColour(wxColour(234, 53, 34)); // #EA3522
    m_error_label->SetWindowStyle(wxALIGN_CENTER_VERTICAL | wxALIGN_CENTER_HORIZONTAL);
    m_error_label->SetMinSize(wxSize(348,55));

    page1Sizer->Add(m_error_label, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);
    m_error_label->Show(false); 

    //login button
    m_login_button_page1 = new FFButton(parent, wxID_ANY,_L("Login"));
    //m_login_button_page1->SetFontDisableColor(wxColour(255, 255, 255));
    //m_login_button_page1->SetBorderDisableColor(wxColour(221,221,221));
    //m_login_button_page1->SetFontColor(wxColour(255, 255, 255));
    //m_login_button_page1->SetBorderColor(wxColour(50,141,251));

    m_login_button_page1->SetForegroundColour(wxColour(255, 255, 255));
    m_login_button_page1->SetBackgroundColour(wxColour(221,221,221)); 
    //m_login_button_page1->SetWindowStyleFlag(wxBORDER_NONE); 
    m_login_button_page1->SetMinSize(wxSize(101,44));
    m_login_button_page1->SetFont((wxFont(wxFontInfo(16))));
    m_login_button_page1->Bind(wxEVT_BUTTON,&LoginDialog::onPage1Login, this);

    page1Sizer->Add(m_login_button_page1, 0, wxALIGN_CENTER_HORIZONTAL| wxUP, FromDIP(36));

    //check box
    wxBoxSizer* checkbox_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_checkbox_page1 = new wxPanel(parent, wxID_ANY,wxDefaultPosition,wxSize(FromDIP(209), -1), wxTAB_TRAVERSAL);

    m_page1_checkBox = new FFCheckBox(m_panel_checkbox_page1, wxID_ANY);
    m_page1_checkBox->SetValue(false);
    m_page1_checkBox->Bind(wxEVT_CHECKBOX, &LoginDialog::onAgreeCheckBoxChangedPage1, this);

    m_protocol_page1 = new  wxStaticText(m_panel_checkbox_page1, wxID_ANY,_L("Read and Agree to Accept"));
    m_protocol_page1->SetFont((wxFont(wxFontInfo(14))));

    //Service Item
    m_service_link_page1 = new wxHyperlinkCtrl(m_panel_checkbox_page1, wxID_ANY, _L("《Term of Sevrvice》"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE);
    m_service_link_page1->Bind(wxEVT_HYPERLINK, [this](wxCommandEvent& e){
        wxString url = "http://dev.auth.flashforge.shop/en/userAgreement";
        AppConfig *app_config = wxGetApp().app_config;
        if(app_config){
            std::string language = app_config->get("language");
            if(language.compare("zh_CN") == 0){
                url = "http://dev.auth.flashforge.shop/userAgreement";
            }
        }
        wxLaunchDefaultBrowser(url);
    });

    //privacy Policy
    m_privacy_policy_page1 = new wxHyperlinkCtrl(m_panel_checkbox_page1, wxID_ANY, _L("《Privacy Policy》"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE);
    m_privacy_policy_page1->Bind(wxEVT_HYPERLINK, [this](wxCommandEvent& e){
        wxString url = "http://dev.auth.flashforge.shop/en/privacyPolicy";
        AppConfig *app_config = wxGetApp().app_config;
        if(app_config){
            std::string language = app_config->get("language");
            if(language.compare("zh_CN") == 0){
                url = "http://dev.auth.flashforge.shop/privacyPolicy";
            }
        }
        wxLaunchDefaultBrowser(url);
    });

    //left gaption
    checkbox_sizer->Add(FromDIP(50), 0, 0, 0);
    checkbox_sizer->Add(m_page1_checkBox, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, FromDIP(5));
    checkbox_sizer->Add(m_protocol_page1,0, wxALIGN_CENTER_HORIZONTAL | wxTOP, FromDIP(5));
    checkbox_sizer->Add(m_service_link_page1,0, wxTOP, FromDIP(5));
    checkbox_sizer->Add(m_privacy_policy_page1,0, wxTOP, FromDIP(5));

    m_panel_checkbox_page1->SetSizer(checkbox_sizer);
    m_panel_checkbox_page1->Layout();
    checkbox_sizer->Fit(m_panel_checkbox_page1);

    page1Sizer->Add(m_panel_checkbox_page1, 0, wxEXPAND, 0);
    //page1Sizer->Add(m_service_link_page1, 0,  wxTOP, FromDIP(5));
    //page1Sizer->Add(m_privacy_policy_page1, 0,  wxTOP, FromDIP(5));
}

void LoginDialog::setupLayoutPage2(wxBoxSizer* page2Sizer,wxPanel* parent)
{
    wxPanel* panel = new wxPanel(parent, wxID_ANY);

    //phone number / email
    // bitmap
    wxBitmap usr_pic = create_scaled_bitmap("login-usr", this, 18);

    //left space
    wxPanel* usr_name_space1 = new wxPanel(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER | wxTAB_TRAVERSAL);
    usr_name_space1->SetMinSize(wxSize(66, -1));
    //right space
    wxPanel* usr_name_space2 = new wxPanel(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER | wxTAB_TRAVERSAL);
    usr_name_space2->SetMinSize(wxSize(86, -1));

    wxStaticBoxSizer* border_sizer = new wxStaticBoxSizer(wxVERTICAL, parent, wxEmptyString);

    m_usrname_page2 = new UsrnameTextCtrl(usr_pic,parent,wxID_ANY);
    m_usrname_page2->Bind(wxEVT_TEXT, &LoginDialog::onUsrNameOrPasswordChangedPage2, this);

    //add border
    border_sizer->Add(m_usrname_page2);
    border_sizer->GetStaticBox()->SetSizeHints(-1, -1, -1, 100);

    //adjust layout
    wxBoxSizer *last_sizer = new wxBoxSizer(wxHORIZONTAL);
    //last_sizer->SetMinSize(250,-1);

    last_sizer->Add(usr_name_space1, 0, wxEXPAND|wxLeft, 0);
    last_sizer->Add(border_sizer, 0, wxALL, 0);
    last_sizer->Add(usr_name_space2, 0, wxEXPAND ,0);

    page2Sizer->Add(0,FromDIP(23),0, 0);
    page2Sizer->Add(last_sizer, 0, wxEXPAND ,0);

//*******password******** 
    wxBitmap login_lock_pic = create_scaled_bitmap("login-lock", this, 18);
    wxBitmap login_eye_off_pic = create_scaled_bitmap("login-eye-off", this, 18);
    wxBitmap login_eye_on_pic = create_scaled_bitmap("login-eye-on", this, 18);

    m_password = new PasswordTextCtrl(login_lock_pic,login_eye_off_pic,login_eye_on_pic,parent,wxID_ANY);
    m_password->Bind(wxEVT_TEXT, &LoginDialog::onUsrNameOrPasswordChangedPage2, this);
    wxStaticBoxSizer* verify_border_sizer = new wxStaticBoxSizer(wxVERTICAL, parent, wxEmptyString);
    verify_border_sizer->SetMinSize(wxSize(190,45));
    verify_border_sizer->Add(m_password);
    
    //add border
    verify_border_sizer->GetStaticBox()->SetSizeHints(-1, -1, -1, 100);

    //adjust layout
    wxBoxSizer *verify_last_sizer = new wxBoxSizer(wxHORIZONTAL);
    verify_last_sizer->Add(usr_name_space1, 0, wxEXPAND|wxLeft, 0);
    verify_last_sizer->Add(verify_border_sizer);
    verify_last_sizer->Add(usr_name_space2, 0, wxEXPAND ,0);

    page2Sizer->Add(verify_last_sizer, 0, wxEXPAND ,0);

    //register / forget password
    wxHyperlinkCtrl* register_link = new wxHyperlinkCtrl(parent, wxID_ANY, _L("Register"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE);
    register_link->Bind(wxEVT_HYPERLINK, [this](wxCommandEvent& e){
        wxString url = "https://www.baidu.com/";
        wxLaunchDefaultBrowser(url);
    });

    wxHyperlinkCtrl* forget_password_link = new wxHyperlinkCtrl(parent, wxID_ANY, _L("Forget Password"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE);
    forget_password_link->Bind(wxEVT_HYPERLINK, [this](wxCommandEvent& e){
        wxString url = "https://www.youku.com/";
        wxLaunchDefaultBrowser(url);
    });

    wxBoxSizer *regist_forget_hor_sizer = new wxBoxSizer(wxHORIZONTAL);
    regist_forget_hor_sizer->SetMinSize(250,-1);

    regist_forget_hor_sizer->Add(usr_name_space1, 0, wxEXPAND|wxLeft, 0);
    regist_forget_hor_sizer->Add(register_link, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, FromDIP(10));
    regist_forget_hor_sizer->AddSpacer(135);
    regist_forget_hor_sizer->Add(forget_password_link, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, FromDIP(10));

    page2Sizer->Add(regist_forget_hor_sizer, 0, wxEXPAND | wxALIGN_CENTER_HORIZONTAL,0);

//****error tips ***
    m_error_label_page2 = new wxStaticText(parent,wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize);
    m_error_label_page2->SetLabel(_L("Account or Password Input Error"));
    m_error_label_page2->SetFont((wxFont(wxFontInfo(16))));
    m_error_label_page2->SetBackgroundColour(wxColour(250, 207, 202)); // #FACFCA
    m_error_label_page2->SetForegroundColour(wxColour(234, 53, 34)); // #EA3522
    m_error_label_page2->SetWindowStyle(wxALIGN_CENTER_VERTICAL | wxALIGN_CENTER_HORIZONTAL);
    m_error_label_page2->SetMinSize(wxSize(348,55));
    page2Sizer->Add(m_error_label_page2, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);
    m_error_label_page2->Show(false); 

    //login button
    m_login_button_page2 = new FFButton(parent, wxID_ANY,_L("Login"));
    m_login_button_page2->SetMinSize(wxSize(101,44));
    m_login_button_page2->SetFont((wxFont(wxFontInfo(16))));
    m_login_button_page2->SetForegroundColour(wxColour(255, 255, 255));
    m_login_button_page2->SetBackgroundColour(wxColour(221,221,221)); 
    m_login_button_page2->SetWindowStyleFlag(wxBORDER_NONE); 
    m_login_button_page2->Bind(wxEVT_BUTTON,&LoginDialog::onPage2Login, this);

    page2Sizer->Add(m_login_button_page2, 0, wxALIGN_CENTER_HORIZONTAL | wxUP, FromDIP(30));

    //check box
    wxBoxSizer* checkbox_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_panel_checkbox_page2 = new wxPanel(parent, wxID_ANY,wxDefaultPosition,wxSize(FromDIP(209), -1), wxTAB_TRAVERSAL);

    m_page2_checkBox = new FFCheckBox(m_panel_checkbox_page2, wxID_ANY);
    m_page2_checkBox->SetValue(false);
    m_page2_checkBox->Bind(wxEVT_CHECKBOX, &LoginDialog::onAgreeCheckBoxChangedPage2, this);

    m_protocol_page2 = new  wxStaticText(m_panel_checkbox_page2, wxID_ANY,_L("Read and Agree to Accept"));
    m_protocol_page2->SetFont((wxFont(wxFontInfo(14))));
    //protocol->SetMinSize(wxSize(46,28));

    //Service Item
    m_service_link_page2 = new wxHyperlinkCtrl(m_panel_checkbox_page2, wxID_ANY, _L("《Term of Sevrvice》"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE);
    m_service_link_page2->Bind(wxEVT_HYPERLINK, [this](wxCommandEvent& e){
        wxString url = "http://dev.auth.flashforge.shop/en/userAgreement";
        AppConfig *app_config = wxGetApp().app_config;
        if(app_config){
            std::string language = app_config->get("language");
            if(language.compare("zh_CN") == 0){
                url = "http://dev.auth.flashforge.shop/userAgreement";
            }
        }
        wxLaunchDefaultBrowser(url);
    });

    //privacy Policy
    m_privacy_policy_page2 = new wxHyperlinkCtrl(m_panel_checkbox_page2, wxID_ANY, _L("《Privacy Policy》"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE);
    m_privacy_policy_page2->Bind(wxEVT_HYPERLINK, [this](wxCommandEvent& e){
        wxString url = "http://dev.auth.flashforge.shop/en/privacyPolicy";
        AppConfig *app_config = wxGetApp().app_config;
        if(app_config){
            std::string language = app_config->get("language");
            if(language.compare("zh_CN") == 0){
                url = "http://dev.auth.flashforge.shop/privacyPolicy";
            }
        }
        wxLaunchDefaultBrowser(url);
    });

    //left gaption
    checkbox_sizer->Add(FromDIP(50), 0, 0, 0);
    checkbox_sizer->Add(m_page2_checkBox, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, FromDIP(5));
    checkbox_sizer->Add(m_protocol_page2,0, wxALIGN_CENTER_HORIZONTAL | wxTOP, FromDIP(5));
    checkbox_sizer->Add(m_service_link_page2,0, wxTOP, FromDIP(5));
    checkbox_sizer->Add(m_privacy_policy_page2,0, wxTOP, FromDIP(5));

    m_panel_checkbox_page2->SetSizer(checkbox_sizer);
    m_panel_checkbox_page2->Layout();
    checkbox_sizer->Fit(m_panel_checkbox_page2);

    page2Sizer->Add(m_panel_checkbox_page2, 0, wxEXPAND, 0);
}

void LoginDialog::onUsrNameOrPasswordChangedPage1(wxCommandEvent& event)
{
    //
        wxString username = m_usrname_page1->GetValue();
        wxString verifycode = m_verify_code->GetValue();
        bool agree = m_page1_checkBox->GetValue();
        if (!username.IsEmpty() && !verifycode.IsEmpty() && agree)
        {
            m_login_button_page1->Enable();
            m_login_button_page1->SetForegroundColour(wxColour(255, 255, 255));
            m_login_button_page1->SetBackgroundColour(wxColour(50,141,251)); 
        }
        else{
            m_login_button_page1->Disable();
            m_login_button_page1->SetForegroundColour(wxColour(255, 255, 255));
            m_login_button_page1->SetBackgroundColour(wxColour(221,221,221)); 
        }
}

void LoginDialog::onAgreeCheckBoxChangedPage1(wxCommandEvent& event)
{
        wxString username = m_usrname_page1->GetValue();
        wxString verifycode = m_verify_code->GetValue();
        bool agree = m_page1_checkBox->GetValue();
        if (!username.IsEmpty() && !verifycode.IsEmpty() && agree)
        {
            m_login_button_page1->Enable();
            m_login_button_page1->SetForegroundColour(wxColour(255, 255, 255));
            m_login_button_page1->SetBackgroundColour(wxColour(50,141,251)); 
        }
        else{
            m_login_button_page1->Disable();
            m_login_button_page1->SetForegroundColour(wxColour(255, 255, 255));
            m_login_button_page1->SetBackgroundColour(wxColour(221,221,221)); 
        }
}

void LoginDialog::onUsrNameOrPasswordChangedPage2(wxCommandEvent& event)
{
        wxString username = m_usrname_page2->GetValue();
        wxString password = m_password->GetValue();
        bool agree = m_page2_checkBox->GetValue();
        if (!username.IsEmpty() && !password.IsEmpty() && agree)
        {
            m_login_button_page2->Enable();
            m_login_button_page2->SetForegroundColour(wxColour(255, 255, 255));
            m_login_button_page2->SetBackgroundColour(wxColour(50,141,251));
        }
        else{
            m_login_button_page2->Disable();
            m_login_button_page2->SetForegroundColour(wxColour(255, 255, 255));
            m_login_button_page2->SetBackgroundColour(wxColour(221,221,221)); 
        }
}

void LoginDialog::onAgreeCheckBoxChangedPage2(wxCommandEvent& event)
{
        wxString username = m_usrname_page2->GetValue();
        wxString password = m_password->GetValue();
        bool agree = m_page2_checkBox->GetValue();
        if (!username.IsEmpty() && !password.IsEmpty() && agree)
        {
            m_login_button_page2->Enable();
            m_login_button_page2->SetForegroundColour(wxColour(255, 255, 255));
            m_login_button_page2->SetBackgroundColour(wxColour(50,141,251)); 
        }
        else{
            m_login_button_page2->Disable();
            m_login_button_page2->SetForegroundColour(wxColour(255, 255, 255));
            m_login_button_page2->SetBackgroundColour(wxColour(221,221,221)); 
        }
}

void LoginDialog::onPage1Login(wxCommandEvent& event)
{
    wxString usrname = m_usrname_page1->GetValue();
    wxString verify_code = m_verify_code->GetValue();
    com_token_data_t token_data;
    ComErrno login_result =  MultiComUtils::getTokenBySMSCode(usrname.ToStdString(),verify_code.ToStdString(),token_data);
    if(login_result == ComErrno::COM_OK){
        LoginDialog::m_token_data = token_data;
        wxGetApp().handle_login_result("default.jpg",usrname.ToStdString());
        this->Hide();
        AppConfig *app_config = wxGetApp().app_config;
        if(app_config){
            //主动点击登录，设置token值
            app_config->set("access_token",token_data.accessToken);
            app_config->set("refresh_token",token_data.refreshToken);
            app_config->set("expire_time",std::to_string(token_data.expiresIn));
             Slic3r::GUI::MultiComMgr::inst()->setWanDevToken(usrname.ToStdString(),token_data.accessToken);
        } 
    }
    else if (login_result == ComErrno::COM_INVALID_VALIDATION){
        m_timer.Bind(wxEVT_TIMER, &LoginDialog::OnTimer, this);
        //账号、验证码错误
        m_error_label->Show(true);
        startTimer();
    }
}

void LoginDialog::onPage2Login(wxCommandEvent& event)
{
    wxString usrname = m_usrname_page2->GetValue();
    wxString password = m_password->GetValue();
    com_token_data_t token_data;
    ComErrno login_result =  MultiComUtils::getTokenByPassword(usrname.ToStdString(),password.ToStdString(),token_data);
    if(login_result == ComErrno::COM_OK){
        LoginDialog::m_token_data = token_data;
        wxGetApp().handle_login_result("default.jpg",usrname.ToStdString());
        this->Hide();
        AppConfig *app_config = wxGetApp().app_config;
        if(app_config){
            //主动点击登录，设置token值
            app_config->set("access_token",token_data.accessToken);
            app_config->set("refresh_token",token_data.refreshToken);
            app_config->set("expire_time",std::to_string(token_data.expiresIn));
             Slic3r::GUI::MultiComMgr::inst()->setWanDevToken(usrname.ToStdString(),token_data.accessToken);
        }
        
    }
    else if (login_result == ComErrno::COM_INVALID_VALIDATION){
        m_timer.Bind(wxEVT_TIMER, &LoginDialog::OnTimer, this);
        //账号、密码错误
        m_error_label_page2->Show(true);
        startTimer();
    }
}

void LoginDialog::OnTimer(wxTimerEvent& event)
{
    if(m_error_label->IsShown()){
        m_error_label->Show(false);
    }
    if(m_error_label_page2->IsShown()){
        m_error_label_page2->Show(false);
    }

    m_timer.Stop();
}


}
}