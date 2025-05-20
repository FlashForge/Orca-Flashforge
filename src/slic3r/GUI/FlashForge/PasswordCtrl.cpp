#include "PasswordCtrl.hpp"

#include "slic3r/GUI/Widgets/TextCtrl.h"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/I18N.hpp"

#include <wx/dcgraph.h>

namespace Slic3r {
namespace GUI {

PasswordCtrl::PasswordCtrl(wxWindow* parent, wxWindowID id/* = wxID_ANY*/, const wxString& value/* = wxEmptyString*/, const wxPoint& pos /*= wxDefaultPosition*/, const wxSize& size/* = wxDefaultSize*/, long style/* = 0*/, const wxValidator& validator /*= wxDefaultValidator*/, const wxString& name /*= wxPanelNameStr*/) 
    : wxPanel(parent, id,pos,size)
    , m_radius(8)
{
    wxBoxSizer* body_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_panel_page = new wxPanel(this, wxID_ANY, wxDefaultPosition,wxSize(FromDIP(270), FromDIP(20)),wxBORDER_NONE);

    m_text_ctrl = new wxTextCtrl(m_panel_page, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD | wxTE_PROCESS_ENTER | wxTE_LEFT | wxBORDER_NONE);
    m_text_ctrl->SetHint(_L("Password"));
    m_text_ctrl->SetMinSize(wxSize(FromDIP(200),FromDIP(20)));
    m_icon = create_scaled_bitmap("login-lock", this, 18);
    m_eye_off_bitmap = create_scaled_bitmap("login-eye-off", this, 10);
    m_eye_on_bitmap = create_scaled_bitmap("login-eye-on", this, 10);

    m_eye_show_icon = new wxStaticBitmap(m_panel_page, wxID_ANY, m_eye_off_bitmap);

    body_sizer->Add(new wxStaticBitmap(m_panel_page, wxID_ANY, m_icon), 0, wxALL, 0);
    body_sizer->AddSpacer(FromDIP(9));
    body_sizer->Add(m_text_ctrl, 0,  wxALIGN_CENTER);
    body_sizer->Add(m_eye_show_icon, wxSizerFlags().Center().Border(wxLEFT, FromDIP(10)));

    m_eye_show_icon->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e){OnShowPasswordButtonClicked(e);});

    m_panel_page->SetSizer(body_sizer);
    m_panel_page->Layout();
    body_sizer->Fit(m_panel_page);

    m_plain_text_ctrl = new wxTextCtrl(m_panel_page, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER | wxTE_LEFT | wxBORDER_NONE);
    m_plain_text_ctrl->SetHint(_L("Password"));
    m_plain_text_ctrl->Hide();

    Bind(wxEVT_PAINT, &PasswordCtrl::OnPaint, this);
    this->SetMinSize(wxSize(FromDIP(276), FromDIP(42)));
}

PasswordCtrl::~PasswordCtrl()
{
    if(m_text_ctrl){
        delete m_text_ctrl;
        m_text_ctrl = nullptr;
    }
    if(m_panel_page){
        delete m_panel_page;
        m_panel_page = nullptr;
    }
}

void PasswordCtrl::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    render(dc);
}

void PasswordCtrl::render(wxDC& dc)
{
    wxSize size = GetSize();

    dc.SetPen(wxPen(wxColour(153,153,153), 1));
    dc.SetBrush(wxBrush(wxColour(255, 255, 255)));

#ifdef _WIN32
    dc.DrawRoundedRectangle(0, 0, size.x, size.y, m_radius);
#else ifdef __APPLE__
    dc.DrawRoundedRectangle(1, 1, size.x - 1, size.y - 1, m_radius);
#endif

    wxPoint pt = {10, 0};
    int width = m_icon.GetWidth();
    int height = m_icon.GetHeight();
    pt.y = (size.y - height) / 2;
#ifdef __APPLE__
    if(pt.y < 10){
        pt.y = 10;
    }
#endif //APPLE
    m_panel_page->SetPosition(pt);
    m_panel_page->Layout();
    if(!m_encrypt)
    {
        m_eye_show_icon->SetPosition(m_eye_pic_position);
        m_eye_show_icon->Show(true);
    }
}

void PasswordCtrl::SetRadius(int r) 
{ 
    m_radius = r;
    Refresh();
}

wxString PasswordCtrl::GetValue() 
{
    if (m_encrypt) {
        return m_text_ctrl->GetValue();
    } else {
        return m_plain_text_ctrl->GetValue();
    }
}

void PasswordCtrl::RefreshEyePicPosition()
{
    if(!m_encrypt){
        //明文显示时调整眼睛图标位置
        m_eye_show_icon->SetPosition(m_eye_pic_position);
        m_eye_show_icon->Show(true);
        m_eye_show_icon->Refresh();
        m_eye_show_icon->Update();
    }
}

void PasswordCtrl::ShowEncrypt() 
{
    m_encrypt = true;
    m_eye_show_icon->SetBitmap(m_eye_off_bitmap);
    m_plain_text_ctrl->Hide();
    m_text_ctrl->SetValue(wxEmptyString);
    m_text_ctrl->Show();
}

void PasswordCtrl::OnShowPasswordButtonClicked(wxMouseEvent& event)
{
    if (m_encrypt) {
        m_encrypt = false;
        //获取眼睛图标位置(明文显示时，因为密文控件被隐藏，会导致图标显示位置变更)
        m_eye_pic_position = m_eye_show_icon->GetPosition();
        //替换为明文控件
        wxString password = m_text_ctrl->GetValue();
        wxPoint point = m_text_ctrl->GetPosition();
        wxSize size = m_text_ctrl->GetSize();
        m_text_ctrl->Hide();
        m_eye_show_icon->SetBitmap(m_eye_on_bitmap);
        m_plain_text_ctrl->SetPosition(point);
        m_plain_text_ctrl->SetSize(size);
        m_plain_text_ctrl->SetValue(wxEmptyString);
        m_plain_text_ctrl->SetValue(password);
        m_plain_text_ctrl->Show();
    }
    else{
        m_encrypt = true;
        m_eye_show_icon->SetBitmap(m_eye_off_bitmap);
        //替换为密文控件
        wxString plain_text = m_plain_text_ctrl->GetValue();
        m_plain_text_ctrl->Hide();
        m_text_ctrl->SetValue(wxEmptyString);
        m_text_ctrl->SetValue(plain_text);
        m_text_ctrl->Show();
    }

}

void PasswordCtrl::ClearTxt()
{
    m_text_ctrl->Clear();
    m_plain_text_ctrl->Clear();
};

}
}
