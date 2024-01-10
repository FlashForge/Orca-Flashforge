#ifndef slic3r_GUI_LoginDialog_hpp_
#define slic3r_GUI_LoginDialog_hpp_

#include <wx/wx.h>
#include <wx/intl.h>
#include <wx/hyperlink.h>

#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
//#include "slic3r/GUI/Widgets/TextInput.hpp"
//#include "slic3r/GUI/Widgets/StaticLine.hpp"
#include "slic3r/GUI/FlashForge/MultiComDef.hpp"

namespace Slic3r { 
namespace GUI {

//wxDEFINE_EVENT(EVT_SET_FINISH_MAPPING, wxCommandEvent);
wxDECLARE_EVENT(EVT_UPDATE_TEXT_LOGIN, wxCommandEvent);

class CountdownButton : public Button
{
public:
    CountdownButton(wxWindow* parent, wxString text, wxString icon = "", long style = 0, int iconSize = 0, wxWindowID btn_id = wxID_ANY);
    void startTimer(){
        // 启动定时器，每秒钟更新一次按钮上的文本
        m_timer.Start(1000);
    }

private:
    void OnTimer(wxTimerEvent& event);

    wxTimer m_timer;
    int m_countdown;
    wxWindow* m_parent;
};

class VerifycodeTextCtrl : public wxPanel
{
public:
    VerifycodeTextCtrl(wxBitmap verifycodebitmap,wxWindow *parent, wxWindowID id = wxID_ANY);

    wxString GetValue(){
        return m_verify_code_text_ctrl->GetValue();
    }

private:
    wxTextCtrl*      m_verify_code_text_ctrl;
    wxStaticBitmap*  m_verify_staticbitmap;
};

class UsrnameTextCtrl : public wxPanel
{
public:
    UsrnameTextCtrl(wxBitmap usrnamebitmap,wxWindow *parent, wxWindowID id = wxID_ANY);

    wxString GetValue(){
        return m_user_name_text_ctrl->GetValue();
    }

private:
    wxTextCtrl*      m_user_name_text_ctrl;
    wxStaticBitmap*  m_usr_staticbitmap;
};

class PasswordTextCtrl : public wxPanel
{
public:
    PasswordTextCtrl(wxBitmap lockbitmap,wxBitmap eyeoffbitmapBtn,wxBitmap eyeonbitmapBtn,wxWindow *parent, wxWindowID id = wxID_ANY);

    wxString GetValue();

    void RefreshEyePicPosition();

private:
    void OnShowPasswordButtonClicked(wxMouseEvent& event);

private:
    wxTextCtrl*      m_password_text_ctrl{nullptr};
    wxTextCtrl*      m_plain_text_ctrl{nullptr};
    wxBitmap         m_eye_off_bitmap;
    wxBitmap         m_eye_on_bitmap;
    wxStaticBitmap*  m_lock_staticbitmap{nullptr};
    wxStaticBitmap*  m_showPassword_staticbitmap{nullptr};
    bool             m_encrypt = true;
    wxPoint          m_eye_pic_position;
};

class LoginDialog : public DPIDialog
{
public:
    LoginDialog();

    static com_token_data_t GetLoginToken();
    static void SetToken(std::string accessToken, std::string refreshToken);

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;

private:
    void initWidget();

    void setupLayoutPage1(wxBoxSizer* page1Sizer,wxPanel* parent);
    void setupLayoutPage2(wxBoxSizer* page2Sizer,wxPanel* parent);

    void onUsrNameOrPasswordChangedPage1(wxCommandEvent& event);
    void onAgreeCheckBoxChangedPage1(wxCommandEvent& event);

    void onUsrNameOrPasswordChangedPage2(wxCommandEvent& event);
    void onAgreeCheckBoxChangedPage2(wxCommandEvent& event);

    void onPage1Login(wxCommandEvent& event);
    void onPage2Login(wxCommandEvent& event);

    inline void startTimer(){ m_timer.Start(5000);}
    void OnTimer(wxTimerEvent& event);

private:
    com_clinet_token_data_t m_client_SMS_token;

    wxStaticText* m_error_label {nullptr};
    wxButton* m_login_button_page1 {nullptr};
    UsrnameTextCtrl* m_usrname_page1 {nullptr};
    VerifycodeTextCtrl* m_verify_code {nullptr};
    wxCheckBox* m_login_check_box_page1 {nullptr};
    CountdownButton* m_get_code_button {nullptr};
    wxStaticText* m_protocol_page1{nullptr};
    wxHyperlinkCtrl* m_service_link_page1{nullptr};
    wxHyperlinkCtrl* m_privacy_policy_page1{nullptr};

    UsrnameTextCtrl* m_usrname_page2 {nullptr};
    PasswordTextCtrl* m_password {nullptr};
    wxStaticText* m_error_label_page2 {nullptr};
    wxButton* m_login_button_page2 {nullptr};
    wxCheckBox* m_login_check_box_page2 {nullptr};
    wxPanel* m_panel_checkbox_page2 {nullptr};

    wxStaticText* m_protocol_page2{nullptr};
    wxHyperlinkCtrl* m_service_link_page2{nullptr};
    wxHyperlinkCtrl* m_privacy_policy_page2{nullptr};

    wxStaticLine* m_staticLine_verify{nullptr};
    wxStaticLine* m_staticLine_password{nullptr};

    //StaticLine* m_staticLine_verify{nullptr};
    //StaticLine* m_staticLine_password{nullptr};
    
    static com_token_data_t  m_token_data;

    wxTimer m_timer;

};
} // namespace GUI
} // namespace Slic3r

#endif