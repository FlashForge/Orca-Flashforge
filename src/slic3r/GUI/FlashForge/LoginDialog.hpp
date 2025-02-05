#ifndef slic3r_GUI_LoginDialog_hpp_
#define slic3r_GUI_LoginDialog_hpp_

#include <wx/wx.h>
#include <wx/intl.h>
#include <wx/hyperlink.h>

#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"
#include "slic3r/GUI/Widgets/FFCheckBox.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/FlashForge/UserNameCtrl.hpp"
#include "slic3r/GUI/FlashForge/VerifyCodeCtrl.hpp"
#include "slic3r/GUI/FlashForge/PasswordCtrl.hpp"
#include "slic3r/GUI/FlashForge/MultiComDef.hpp"
#include "slic3r/GUI/TitleDialog.hpp"

namespace Slic3r { 
namespace GUI {

//wxDEFINE_EVENT(EVT_SET_FINISH_MAPPING, wxCommandEvent);
wxDECLARE_EVENT(EVT_UPDATE_TEXT_LOGIN, wxCommandEvent);

class CountdownButton : public FFButton
{
public:
    CountdownButton(wxWindow* parent, wxString text, wxString icon = "", long style = 0, int iconSize = 0, wxWindowID btn_id = wxID_ANY);
    void startTimer(){
        // 启动定时器，每秒钟更新一次按钮上的文本
        m_timer.Start(1000);
    }
    void SetState(bool state);
    bool GetState();
    void StopTimer();

private:
    void OnTimer(wxTimerEvent& event);

private:
    std::mutex m_mutex;
    wxTimer m_timer;
    int m_countdown;
    bool m_press = false;
};

class LoginDialog : public TitleDialog
{
public:
    LoginDialog();
    ~LoginDialog();

    void ReLoad();

    static com_token_data_t GetLoginToken();
    static void SetToken(const std::string& accessToken, const std::string& refreshToken);
    static void SetUsrLogin(bool loginState);
    static bool IsUsrLogin();
    static void SetUsrInfo(const com_user_profile_t& usrInfo);
    static const com_user_profile_t& GetUsrInfo();
    static const std::string GetUsrName();

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;

private:
    void initWidget();
    void initData();
    void initBindEvent();

    void initOverseaWidget();

    void createBodyWidget();
    void createSwitchTitle();

    void title1Clicked(wxMouseEvent &event);
    void title2Clicked(wxMouseEvent &event);
    void switchTitle1();
    void switchTtitle2();
    void gCodeClicked(wxMouseEvent& event);

    void setupLayoutPage1(wxBoxSizer* page1Sizer,wxPanel* parent);
    void setupLayoutPage2(wxBoxSizer* page2Sizer,wxPanel* parent,bool foreign = false);

    void onUsrNameOrPasswordChangedPage1(wxCommandEvent& event);
    void onAgreeCheckBoxChangedPage1(wxCommandEvent& event);

    void onUsrNameOrPasswordChangedPage2(wxCommandEvent& event);
    void onAgreeCheckBoxChangedPage2(wxCommandEvent& event);

    void onPage1Login(wxMouseEvent& event);
    void onPage2Login(wxMouseEvent &event);

    void page1ShowErrorLabel(const wxString& labelInfo);
    void page2ShowErrorLabel(const wxString& labelInfo);

    inline void startTimer(){ m_timer.Start(2000);}
    void OnTimer(wxTimerEvent& event);

    ComErrno getSmsCode();

private:
    com_clinet_token_data_t m_client_SMS_token;

    wxBoxSizer*	m_sizer_main {nullptr};
    wxBoxSizer* m_page_title_sizer {nullptr};
    wxBoxSizer* m_page_body_sizer {nullptr};

    wxStaticText* m_switch_title_1 {nullptr};
    wxStaticText* m_switch_title_2 {nullptr};

    wxBoxSizer* m_switch_title_1_sizer {nullptr};
    wxBoxSizer* m_switch_title_2_sizer {nullptr};

    wxPanel* m_switch_title_1_panel {nullptr};
    wxPanel* m_switch_title_2_panel {nullptr};

    wxPanel* m_switch_title_1_line_panel {nullptr};
    wxPanel* m_switch_title_2_line_panel {nullptr};

    wxPanel* m_page_body_page1_panel {nullptr};
    wxPanel* m_page_body_page2_panel {nullptr};

    //wxStaticText* m_error_label {nullptr};
    wxPanel* m_error_label_panel {nullptr};
    //Label        *m_error_label{nullptr};
    FFButton *m_error_label{nullptr};

    FFButton* m_login_button_page1 {nullptr};
    CountdownButton* m_get_code_button {nullptr};
    wxStaticText* m_protocol_page1{nullptr};
    wxStaticText* m_service_link_page1 {nullptr};
    wxStaticText    *m_st_and_title1{nullptr};
    wxStaticText* m_privacy_policy_page1 {nullptr};
    wxPanel*         m_panel_checkbox_page0{nullptr};
    wxPanel* m_panel_checkbox_page1 {nullptr};
    wxPanel *m_panel_separotor_login{nullptr};

    //wxStaticText* m_error_label_page2 {nullptr};
    wxPanel* m_error_label_page2_panel {nullptr};
    FFButton     *m_error_label_page2{nullptr};
    wxPanel  *m_panel_separotor_login2{nullptr};

    FFButton* m_login_button_page2 {nullptr};
    wxPanel*  m_panel_checkbox_page3{nullptr};
    wxPanel* m_panel_checkbox_page2 {nullptr};

    wxStaticText* m_protocol_page2{nullptr};
    wxStaticText* m_service_link_page2 {nullptr};
    wxStaticText *m_st_and_title2{nullptr};
    wxStaticText* m_privacy_policy_page2 {nullptr};
    
    wxPanel* m_title_1_underline {nullptr};
    wxPanel* m_title_2_underline {nullptr};

    FFCheckBox*     m_page1_checkBox {nullptr};
    FFCheckBox*     m_page2_checkBox {nullptr};
    
    static com_token_data_t  m_token_data;
    static bool m_usr_is_login;
    static com_user_profile_t m_usr_info;
    static bool  m_first_call_client_token;

    wxTimer m_timer;

    UserNameCtrl* m_username_ctrl_page1 {nullptr};
    VerifyCodeCtrl* m_verifycode_ctrl_page1 {nullptr};
    UserNameCtrl* m_username_ctrl_page2 {nullptr};
    PasswordCtrl* m_password_ctrl_page2 {nullptr};

    static std::string m_usr_name;
    std::string        m_cur_language;
    std::string        m_sms_info;
    bool               m_login1_pressed{false};
    bool               m_login2_pressed{false};

};
} // namespace GUI
} // namespace Slic3r

#endif
