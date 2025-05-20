#include "BindDialog.hpp"
#include "GUI_App.hpp"

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include "wx/evtloop.h"
#include <wx/tokenzr.h>
#include <wx/richmsgdlg.h>
#include <wx/richtext/richtextctrl.h>
#include "libslic3r/Model.hpp"
#include "libslic3r/Polygon.hpp"
#include "MainFrame.hpp"
#include "GUI_App.hpp"
#include "Plater.hpp"
#include "Widgets/WebView.hpp"
#include "FlashForge/MultiComMgr.hpp"
#include "FlashForge/LoginDialog.hpp"
#include "FlashForge/DeviceData.hpp"
#include "Widgets/FFButton.hpp"
#include "slic3r/GUI/FFUtils.hpp"

namespace Slic3r {
namespace GUI {

    const int USER_NAME_LENGTH = 180;

wxString get_fail_reason(int code)
{
    if (code == BAMBU_NETWORK_ERR_BIND_CREATE_SOCKET_FAILED)
        return _L("Failed to create socket");

    else if (code == BAMBU_NETWORK_ERR_BIND_SOCKET_CONNECT_FAILED)
        return _L("Failed to connect socket");

    else if (code == BAMBU_NETWORK_ERR_BIND_PUBLISH_LOGIN_REQUEST)
        return _L("Failed to publish login request");

    else if (code == BAMBU_NETWORK_ERR_BIND_GET_PRINTER_TICKET_TIMEOUT)
        return _L("Get ticket from device timeout");

    else if (code == BAMBU_NETWORK_ERR_BIND_GET_CLOUD_TICKET_TIMEOUT)
        return _L("Get ticket from server timeout");

    else if (code == BAMBU_NETWORK_ERR_BIND_POST_TICKET_TO_CLOUD_FAILED)
        return _L("Failed to post ticket to server");

    else if (code == BAMBU_NETWORK_ERR_BIND_PARSE_LOGIN_REPORT_FAILED)
        return _L("Failed to parse login report reason"); 
    
    else if (code == BAMBU_NETWORK_ERR_BIND_ECODE_LOGIN_REPORT_FAILED)
        return _L("Failed to parse login report reason");

    else if (code == BAMBU_NETWORK_ERR_BIND_RECEIVE_LOGIN_REPORT_TIMEOUT)
        return _L("Receive login report timeout");

    else
        return _L("Unknown Failure");
}

BindMachineDialog::LinkLabel::LinkLabel(wxWindow *parent, const wxString &text, const wxString& link)
    : Label(parent, text)
    , m_link(link)
{
    //SetFont(Label::Head_13);
    SetMaxSize(wxSize(FromDIP(450), -1));
    Wrap(FromDIP(450));
    SetForegroundColour(wxColour("#328DFB"));
    Bind(wxEVT_LEFT_DOWN, [this](auto& e) {
        wxLaunchDefaultBrowser(m_link);
    });
    Bind(wxEVT_ENTER_WINDOW, [this](auto& e) {
        SetForegroundColour(wxColour("#95C5FF"));
        Refresh();
        SetCursor(wxCURSOR_HAND);
    });
    Bind(wxEVT_LEAVE_WINDOW, [this](auto& e) {
        SetForegroundColour(wxColour("#328DFB"));
        Refresh();
        SetCursor(wxCURSOR_ARROW);
    });
}

RoundImagePanel::RoundImagePanel(wxWindow *parent, const wxSize& size/*=wxDefaultSize*/)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
{
    Bind(wxEVT_PAINT, &RoundImagePanel::OnPaint, this);
    //Bind(wxEVT_SIZE, &RoundImagePanel::OnSize, this);
}

void RoundImagePanel::SetImage(const wxImage &image)
{
    m_image = image;
    Refresh();
}

void RoundImagePanel::OnSize(wxSizeEvent& event)
{

}

void RoundImagePanel::OnPaint(wxPaintEvent& event)
{
    if (!m_image.IsOk()) {
        event.Skip();
        return;
    }

    wxSize size = GetSize();
    wxImage img = m_image;
    img.Rescale(size.x, size.y);
    if (!img.HasAlpha()) {
        img.InitAlpha();
    }
    wxPaintDC dc(this);
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

void RoundImagePanel::CreateRegion(wxDC &dc)
{
    wxSize sz = GetSize();
    int x = sz.x / 2;
    int y = sz.y / 2;
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(0, 0, sz.x, sz.y);
    //dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetPen(wxColor("#000000"));
    dc.SetBrush(*wxRED);
    dc.DrawCircle(x, y, (x < y) ? x : y);
}


BindMachineDialog::BindMachineDialog()
    : TitleDialog(static_cast<wxWindow *>(wxGetApp().mainframe), _L("Register printer"), 6)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__

    SetBackgroundColour(*wxWHITE);

    m_simplebook = new wxSimplebook(this);

    m_normal_panel = new wxPanel(m_simplebook);
    wxBoxSizer* normal_sizer = new wxBoxSizer(wxVERTICAL);

    m_top_panel = new wxPanel(m_normal_panel);

    m_machine_sizer = new wxBoxSizer(wxVERTICAL);     
    m_printer_img = new wxStaticBitmap(m_top_panel, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize(FromDIP(120), FromDIP(120)), 0);
    m_printer_name = new wxStaticText(m_top_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize);
    m_printer_name->SetMaxSize(wxSize(FromDIP(300), -1));
    m_printer_name->Wrap(FromDIP(300));
    m_printer_name->SetForegroundColour(wxColor("#333333"));
    m_printer_name->SetFont(GetFont());
    m_machine_sizer->AddStretchSpacer(1);
    m_machine_sizer->Add(m_printer_img, 0, wxALIGN_CENTER, 0);
    m_machine_sizer->AddSpacer(FromDIP(5));
    m_machine_sizer->Add(m_printer_name, 0, wxALIGN_CENTER, 0);
    m_machine_sizer->AddStretchSpacer(1);

    m_user_name = new wxStaticText(m_top_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize);
    m_user_name->SetForegroundColour(wxColor("#333333"));
    m_user_name->SetMaxSize(wxSize(FromDIP(300), -1));
    m_user_name->Wrap(FromDIP(300));    
    
    m_user_sizer = new wxBoxSizer(wxVERTICAL);

    //m_user_img = new wxStaticBitmap(m_normal_panel, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize(FromDIP(80), FromDIP(80)), 0);
    m_user_panel = new RoundImagePanel(m_top_panel, wxSize(FromDIP(80), FromDIP(80)));
    m_user_sizer->AddStretchSpacer(1);
    m_user_sizer->Add(m_user_panel, 0, wxALIGN_CENTER, 0);
    m_user_sizer->AddSpacer(FromDIP(10));
    m_user_sizer->AddStretchSpacer(1);
    m_user_sizer->Add(m_user_name, 0, wxALIGN_CENTER, 0);

    auto m_bind_icon = create_scaled_bitmap("ff_bind_machine", nullptr, 23);
    auto linkBitmap = new wxStaticBitmap(m_top_panel, wxID_ANY, m_bind_icon, wxDefaultPosition, wxSize(FromDIP(23), FromDIP(23)), 0);

    wxBoxSizer *topSizer = new wxBoxSizer(wxHORIZONTAL);
    topSizer->Add(m_machine_sizer, 1, wxEXPAND | wxALIGN_CENTER);
    topSizer->Add(linkBitmap, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(20));
    topSizer->Add(m_user_sizer, 1, wxEXPAND | wxALIGN_CENTER, 0);
    m_top_panel->SetSizer(topSizer);
    m_top_panel->Layout();

    m_bind_text = new wxStaticText(m_normal_panel, wxID_ANY, _L("Would you like to register the printer to this account?"));
    m_bind_text->SetForegroundColour(wxColour("#333333"));
    m_bind_text->SetFont(::Label::Body_14);
    m_bind_text->Wrap(-1);

    // agreement
    m_panel_agreement = new wxWindow(m_normal_panel,wxID_ANY);
    m_panel_agreement->SetBackgroundColour(*wxWHITE);
    m_panel_agreement->SetMinSize(wxSize(FromDIP(450), -1));
    m_panel_agreement->SetMaxSize(wxSize(FromDIP(450), -1));
    
    m_checkbox_privacy = new FFCheckBox(m_panel_agreement);
    m_checkbox_privacy->SetValue(false);
    m_checkbox_privacy->Bind(wxEVT_TOGGLEBUTTON, [this](auto& e) { m_bind_btn->Enable(e.IsChecked()); e.Skip(); });

    wxWrapSizer* sizer_privacy_agreement =  new wxWrapSizer( wxHORIZONTAL, wxWRAPSIZER_DEFAULT_FLAGS );
    auto st_privacy_title = new Label(m_panel_agreement, _L("Read and accept"));
    st_privacy_title->SetFont(GetFont());
    st_privacy_title->SetForegroundColour(wxColour("#333333"));

    m_terms_title   = new LinkLabel(m_panel_agreement, _L("Term of Service"), FFUtils::userAgreement());
    m_privacy_title = new LinkLabel(m_panel_agreement, _L("Privacy Policy"), FFUtils::privacyPolicy());

    auto st_and_title = new Label(m_panel_agreement, _L("and"));
    st_and_title->SetFont(GetFont());
    st_and_title->SetForegroundColour(wxColour("#333333"));

    wxWrapSizer* sizere_notice_agreement=  new wxWrapSizer( wxHORIZONTAL, wxWRAPSIZER_DEFAULT_FLAGS );
    sizere_notice_agreement->Add(0, 0, 0, wxTOP, FromDIP(4));
    sizer_privacy_agreement->Add(st_privacy_title, 0, wxALIGN_CENTER, 0);
    sizer_privacy_agreement->Add(0, 0, 0, wxLEFT, FromDIP(5));
    sizer_privacy_agreement->Add(m_terms_title, 0, wxALIGN_CENTER, 0);
    sizer_privacy_agreement->Add(st_and_title, 0, wxALIGN_CENTER|wxLEFT|wxRIGHT, FromDIP(5));
    sizer_privacy_agreement->Add(m_privacy_title, 0, wxALIGN_CENTER, 0);

    wxBoxSizer* sizer_privacy_body = new wxBoxSizer(wxHORIZONTAL);
    sizer_privacy_body->Add(m_checkbox_privacy, 0, wxALL, 0);
    sizer_privacy_body->Add(0, 0, 0, wxLEFT, FromDIP(8));
    sizer_privacy_body->Add(sizer_privacy_agreement, 1, wxEXPAND, 0);
    m_panel_agreement->SetSizer(sizer_privacy_body);
    m_panel_agreement->Layout();
    m_panel_agreement->Fit();
    
    m_bind_btn = new FFButton(m_normal_panel, wxID_ANY, _L("Confirm"), 4, false);
    m_bind_btn->SetFontUniformColor(wxColour("#ffffff"));
    m_bind_btn->SetBGColor(wxColour("#419488"));
    m_bind_btn->SetBGHoverColor(wxColour("#65A79E"));
    m_bind_btn->SetBGPressColor(wxColour("#1A8676"));
    m_bind_btn->SetBGDisableColor(wxColour("#dddddd"));
    m_bind_btn->SetSize(-1, FromDIP(30));
    m_bind_btn->SetMinSize(wxSize(100, FromDIP(30)));
    m_bind_btn->Enable(false);
    m_cancel_btn = new FFButton(m_normal_panel, wxID_ANY, _L("Cancel"), 4, true);
    m_cancel_btn->SetFontColor(wxColour("#333333"));
    m_cancel_btn->SetFontHoverColor(wxColour("#65A79E"));
    m_cancel_btn->SetFontPressColor(wxColour("#1A8676"));
    m_cancel_btn->SetFontDisableColor(wxColour("#dddddd"));    
    m_cancel_btn->SetBorderColor(wxColour("#333333"));
    m_cancel_btn->SetBorderHoverColor(wxColour("#65A79E"));
    m_cancel_btn->SetBorderPressColor(wxColour("#1A8676"));
    m_cancel_btn->SetBorderDisableColor(wxColour("#dddddd"));
    m_cancel_btn->SetBGUniformColor(wxColour("#ffffff"));
    m_cancel_btn->SetSize(-1, FromDIP(30));
    m_cancel_btn->SetMinSize(wxSize(100, FromDIP(30)));

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->AddStretchSpacer(1);
    btnSizer->Add(m_cancel_btn, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    btnSizer->AddSpacer(FromDIP(50));
    btnSizer->Add(m_bind_btn, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT);
    btnSizer->AddStretchSpacer(1);
    
    normal_sizer->AddSpacer(FromDIP(20));
    normal_sizer->Add(m_top_panel, 0, wxEXPAND | wxALIGN_CENTER | wxLEFT | wxRIGHT, FromDIP(30));
    normal_sizer->AddSpacer(FromDIP(30));
    normal_sizer->Add(m_bind_text, 0, wxEXPAND | wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT, FromDIP(30));
    normal_sizer->Add(0, 0, 0, wxTOP, FromDIP(10));
    normal_sizer->Add(m_panel_agreement, 0, wxEXPAND | wxALIGN_LEFT | wxLEFT | wxRIGHT, FromDIP(30));
    normal_sizer->Add(0, 0, 0, wxTOP, FromDIP(20));
    normal_sizer->Add(btnSizer, 0, wxEXPAND | wxALIGN_CENTER_HORIZONTAL);
    normal_sizer->Add(0, 0, 0, wxTOP, FromDIP(20));
    m_normal_panel->SetSizer(normal_sizer);
    m_normal_panel->Layout();
    m_normal_panel->Fit();

    m_simplebook->AddPage(m_normal_panel, wxEmptyString, true);

    // result panel
    m_result_panel = new wxPanel(m_simplebook);    
    m_result_text = new wxStaticText(m_result_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    m_result_text->SetFont(Label::Body_16);
    m_result_btn = new FFButton(m_result_panel, wxID_ANY, _L("OK"), 4, false);
    m_result_btn->SetFontUniformColor(*wxWHITE);
    m_result_btn->SetBGColor(wxColour("#419488"));
    m_result_btn->SetBGHoverColor(wxColour("#65A79E"));
    m_result_btn->SetBGPressColor(wxColour("#1A8676"));
    m_result_btn->SetSize(wxSize(-1, FromDIP(30)));
    m_result_btn->SetMinSize(wxSize(FromDIP(60), FromDIP(30)));
    m_result_btn->SetMaxSize(wxSize(- 1, FromDIP(30)));
    m_result_btn->Bind(wxEVT_BUTTON, &BindMachineDialog::on_result_ok, this);

    m_result_sizer = new wxBoxSizer(wxVERTICAL);
    m_result_sizer->AddSpacer(FromDIP(40));
    m_result_sizer->AddStretchSpacer(1);
    m_result_sizer->Add(m_result_text, 0, wxALIGN_CENTER);
    m_result_sizer->AddSpacer(FromDIP(80));
    m_result_sizer->Add(m_result_btn, 0, wxALIGN_CENTER | wxALIGN_BOTTOM);
    m_result_sizer->AddStretchSpacer(1);
    m_result_sizer->AddSpacer(FromDIP(10));
    m_result_panel->SetSizer(m_result_sizer);
    m_result_panel->Layout();
    m_result_panel->Fit();
    //m_result_panel->SetBackgroundColour(wxColour("#ffff00"));
    m_simplebook->AddPage(m_result_panel, wxEmptyString, false);   
         
    wxBoxSizer *mainSizer = MainSizer();//new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(m_simplebook, 1, wxEXPAND | wxALL, 0);

    //mainSizer->Fit(this);
    //SetSizer(m_sizer_main);
    Layout();
    Fit();
    Centre(wxBOTH);
    #if 0
    Bind(wxEVT_WEBREQUEST_STATE, [this](wxWebRequestEvent& evt) {
         switch (evt.GetState()) {
         case wxWebRequest::State_Completed: {
             BOOST_LOG_TRIVIAL(error) << "BindDialog: web request state completed";
             wxImage avatar_stream = *evt.GetResponse().GetStream();
             if (avatar_stream.IsOk()) {
                 avatar_stream.Rescale(FromDIP(80), FromDIP(80));
                 //auto bitmap = new wxBitmap(avatar_stream);
                 //bitmap->SetSize(wxSize(FromDIP(60), FromDIP(60)));
                 //m_user_img->SetBitmap(*bitmap);
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
    Bind(wxEVT_SHOW, &BindMachineDialog::on_show, this);
    Bind(wxEVT_CLOSE_WINDOW, &BindMachineDialog::on_close, this);
    m_bind_btn->Bind(wxEVT_BUTTON, &BindMachineDialog::on_bind_printer, this);
    m_cancel_btn->Bind(wxEVT_BUTTON, &BindMachineDialog::on_cancel, this);
    Bind(EVT_BIND_MACHINE_SUCCESS, &BindMachineDialog::on_bind_success, this);
    Bind(EVT_BIND_MACHINE_FAIL, &BindMachineDialog::on_bind_fail, this);

    wxGetApp().UpdateDlgDarkUI(this);
}

BindMachineDialog::~BindMachineDialog()
{
    Unbind(EVT_BIND_MACHINE_SUCCESS, &BindMachineDialog::on_bind_success, this);
    Unbind(EVT_BIND_MACHINE_FAIL, &BindMachineDialog::on_bind_fail, this);
    if (m_bind_info) {
        delete m_bind_info;
        m_bind_info = nullptr;
    }
}

void BindMachineDialog::on_cancel(wxCommandEvent &event)
{
    on_destroy();
    EndModal(wxID_CANCEL);
}

void BindMachineDialog::on_destroy()
{
    /*
    if (m_bind_job) {
        m_bind_job->cancel();
        m_bind_job->join();
    }*/  //by ymd

    //if (m_web_request.IsOk()) {
    //    m_web_request.Cancel();
    //}
}

void BindMachineDialog::on_result_ok(wxCommandEvent& event)
{
    if (m_result_code != 0) {
        m_simplebook->SetSelection(0);
        Layout();
        //Fit();
    } else {
        on_destroy();
        EndModal(wxID_OK);
    }
}

void BindMachineDialog::downloadUrlPic(const std::string& url) 
{
    if (!url.empty()) {
        Slic3r::Http http   = Slic3r::Http::get(url);
        std::string  suffix = url.substr(url.find_last_of(".") + 1);
        http.header("accept", "image/" + suffix)
            .on_complete([this](std::string body, unsigned int status) {
                wxMemoryInputStream stream(body.data(), body.size());
                wxImage             image(stream, wxBITMAP_TYPE_ANY);
                if (!image.IsOk()) {
                    BOOST_LOG_TRIVIAL(error) << "download relogin image is not ok";
                    return;
                }
                wxGetApp().setUsrPic(image);
                image.Rescale(FromDIP(80), FromDIP(80));
                m_user_panel->SetImage(image);
                Layout();
            })
            .on_error([=](std::string body, std::string error, unsigned status) {
                BOOST_LOG_TRIVIAL(info) << " downloadUrlPic: status:" << status << " error:" << error;
            })
            .perform();
    } else {
        wxImage     tmpimage;
        std::string name = "login_default_usr_pic";
        if (tmpimage.LoadFile(Slic3r::GUI::from_u8(Slic3r::var(name + ".png")), wxBITMAP_TYPE_PNG)) {
            wxGetApp().setUsrPic(tmpimage);
            tmpimage.Rescale(FromDIP(80), FromDIP(80));
            m_user_panel->SetImage(tmpimage);
            Layout();
        }
    }
}

void BindMachineDialog::on_close(wxCloseEvent &event)
{
    on_destroy();
    event.Skip();
}

void BindMachineDialog::on_bind_fail(wxCommandEvent &event)
{
    m_result_code = event.GetInt();
    m_result_text->SetLabel(_L("The device registration failed!"));
    m_result_text->SetForegroundColour(wxColor("#EA3522"));
    m_result_sizer->Layout();
    m_simplebook->SetSelection(1);
    m_bind_btn->Enable(true);
    //GetSizer()->Fit(this);
    Layout();
    //Fit();
}

void BindMachineDialog::on_bind_success(wxCommandEvent &event)
{
    m_result_code = 0;
    m_result_text->SetLabel(_L("The device registration successful!"));
    m_result_text->SetForegroundColour(wxColor("#419488"));
    m_result_sizer->Layout();
    m_simplebook->SetSelection(1);
    //GetSizer()->Fit(this);
    Layout();
    //Fit();
    //EndModal(wxID_OK);
    if(m_machine_info) wxGetApp().on_start_subscribe_again(m_machine_info->dev_id);
}

void BindMachineDialog::on_bind_printer(wxCommandEvent &event)
{
    event.Skip();
    m_bind_btn->Enable(false);
    m_result_code = 0;
    #if 0
    //if (!m_device_info) {
    //    BOOST_LOG_TRIVIAL(error) << "device_info is null";
    //    m_bind_btn->Enable(true);
    //    return;
    //}

    //std::string dev_id = m_device_info->get_dev_id();
    //unsigned short dev_pid = m_device_info->get_dev_pid();
    //std::string dev_name = m_device_info->get_dev_name();

    //if (dev_id.empty() || dev_pid == 0) {
    //    BOOST_LOG_TRIVIAL(error) << "dev_id is empty or dev_pid is 0";
    //    //m_bind_btn->Enable(true);
    //    //return;
    //}
    #else
    if (m_bind_info->dev_id.empty() || m_bind_info->dev_pid == 0) {
        BOOST_LOG_TRIVIAL(error) << "dev_id is empty or dev_pid is 0";
    }
    #endif


    BOOST_LOG_TRIVIAL(info) << "on_bind_printer: " << m_bind_info->dev_id
                            << "--dev_ip:" << m_bind_info->dev_ip
                            << "--dev_port:" << m_bind_info->dev_port
                            << "--dev_pid:" << m_bind_info->dev_pid
                            << "--dev_name: " << m_bind_info->dev_name;
    m_bind_job = std::make_shared<BindJob>(m_bind_info->dev_ip, m_bind_info->dev_port,
        m_bind_info->dev_id, m_bind_info->dev_pid, m_bind_info->dev_name);
    m_bind_job->set_event_handle(this);
    m_bind_job->process();
}

void BindMachineDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    m_bind_btn->SetMinSize(BIND_DIALOG_BUTTON_SIZE);
    m_cancel_btn->SetMinSize(BIND_DIALOG_BUTTON_SIZE);
}

//void BindMachineDialog::update_device_info(DeviceObject* info)
//{
//    m_device_info = info;
//}

void BindMachineDialog::update_device_info2(BindInfo *bind_info)
{ 
    m_bind_info = bind_info; 
}

void BindMachineDialog::update_machine_info(MachineObject *info)
{
    m_machine_info = info;
}

void BindMachineDialog::on_show(wxShowEvent &event)
{
    event.Skip();
    m_result_code   = 0;
    if (event.IsShown()) {
        wxBitmap bmp;
        //auto pid = m_device_info->get_dev_pid();
        if (0x0024 == m_bind_info->dev_pid) { // ad 5m pro
            bmp = create_scaled_bitmap("adventurer_5m_pro", 0, 80);
        } else if (0x0023 == m_bind_info->dev_pid) { // ad 5m
            bmp = create_scaled_bitmap("adventurer_5m", 0, 80);
        } else if (0x001F == m_bind_info->dev_pid) { // G3U
            bmp = create_scaled_bitmap("guider_3_ultra", 0, 80);
        } else if (0x0026 == m_bind_info->dev_pid) { // ad5x
            bmp = create_scaled_bitmap("ad5x", 0, 80);
        } else if (0x0025 == m_bind_info->dev_pid) { // Guider4
            bmp = create_scaled_bitmap("guider4", 0, 80);
        }else {
            auto img_path = m_bind_info->img /*m_device_info->get_printer_thumbnail_img_str()*/;
            if (wxGetApp().dark_mode()) { img_path += "_dark"; }
            bmp = create_scaled_bitmap(img_path, this, FromDIP(80));
        }
        m_printer_img->SetBitmap(bmp);
        m_printer_img->Refresh();
        //m_printer_img->Show();

        m_printer_name->SetLabelText(from_u8(m_bind_info->dev_name /*m_device_info->get_dev_name()*/));
        //m_machinePanel->Layout();
        m_machine_sizer->Layout();

        if (LoginDialog::IsUsrLogin()) {
            auto user_info = LoginDialog::GetUsrInfo();
            BOOST_LOG_TRIVIAL(error) << "Get user info: nickname (" << user_info.nickname << "), headImgUrl (" << user_info.headImgUrl << ")";

            wxString username = wxString::FromUTF8(user_info.nickname);
            wxGCDC   dc(this);
            wxString clipName = FFUtils::trimString(dc, username, FromDIP(USER_NAME_LENGTH));            
            m_user_name->SetLabelText(clipName);
            m_user_name->SetToolTip(wxString::FromUTF8(user_info.nickname));
            //m_user_name->SetLabelText(wxString::FromUTF8(user_info.nickname));
            #if 0
            if (!user_info.headImgUrl.empty()) {
                m_web_request = wxWebSession::GetDefault().CreateRequest(this, user_info.headImgUrl);
                if (!m_web_request.IsOk()) {
                    BOOST_LOG_TRIVIAL(error) << "web session create request fail";
                } else {
                    m_web_request.Start();
                }
            }
            #else
            #if 1
            wxImage image = wxGetApp().getUsrPic();
            if (image.IsOk()) {
                image.Rescale(FromDIP(80), FromDIP(80));
                m_user_panel->SetImage(image);
                Layout();
            } else {
                downloadUrlPic(user_info.headImgUrl);
            }
            #endif
            #endif
            m_user_sizer->Layout();
        }
        //m_normal_panel->Fit();
        //m_normal_panel->SetBackgroundColour("#ff0000");
        m_top_panel->Layout();
        m_simplebook->Layout();
        //m_simplebook->Fit();
        Layout();
        //GetSizer()->Fit(this);
        //
        //Fit();
    }
    //event.Skip();
}


UnBindMachineDialog::UnBindMachineDialog()
    : TitleDialog(static_cast<wxWindow *>(wxGetApp().mainframe), _L("Log out printer"), 6)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__

    wxBoxSizer* main_sizer = MainSizer();

    m_machine_sizer = new wxBoxSizer(wxVERTICAL);     
    m_printer_img = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize(FromDIP(120), FromDIP(120)), 0);
    m_printer_name = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize);
    m_printer_name->SetMaxSize(wxSize(FromDIP(350), -1));
    m_printer_name->Wrap(FromDIP(350));
    m_printer_name->SetForegroundColour(wxColor("#333333"));
    m_printer_name->SetFont(GetFont());
    m_machine_sizer->AddStretchSpacer(1);
    m_machine_sizer->Add(m_printer_img, 0, wxALIGN_CENTER, 0);
    m_machine_sizer->AddSpacer(FromDIP(5));
    m_machine_sizer->Add(m_printer_name, 0, wxALIGN_CENTER, 0);
    m_machine_sizer->AddStretchSpacer(1);

    m_user_name = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize);
    m_user_name->SetForegroundColour(wxColor("#333333"));
    m_user_name->SetMaxSize(wxSize(FromDIP(350), -1));
    m_user_name->Wrap(FromDIP(350));
    m_user_sizer = new wxBoxSizer(wxVERTICAL);

    //m_user_img = new wxStaticBitmap(m_normal_panel, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize(FromDIP(80), FromDIP(80)), 0);
    m_user_panel = new RoundImagePanel(this, wxSize(FromDIP(80), FromDIP(80)));
    m_user_sizer->AddStretchSpacer(1);
    m_user_sizer->Add(m_user_panel, 0, wxALIGN_CENTER, 0);
    m_user_sizer->AddSpacer(FromDIP(10));
    m_user_sizer->AddStretchSpacer(1);
    m_user_sizer->Add(m_user_name, 0, wxALIGN_CENTER, 0);

    auto m_bind_icon = create_scaled_bitmap("ff_unbind_machine", nullptr, 23);
    auto linkBitmap = new wxStaticBitmap(this, wxID_ANY, m_bind_icon, wxDefaultPosition, wxSize(FromDIP(23), FromDIP(23)), 0);

    wxBoxSizer *topSizer = new wxBoxSizer(wxHORIZONTAL);
    topSizer->Add(m_machine_sizer, 1, wxEXPAND | wxALIGN_CENTER);
    topSizer->Add(linkBitmap, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(20));
    topSizer->Add(m_user_sizer, 1, wxEXPAND | wxALIGN_CENTER, 0);

    m_unbind_text = new wxStaticText(this, wxID_ANY, _L("Would you like to log the printer out of this account?"));
    m_unbind_text->SetForegroundColour(wxColour("#333333"));
    m_unbind_text->SetFont(::Label::Body_14);
    m_unbind_text->Wrap(-1);
    
    m_unbind_btn = new FFButton(this, wxID_ANY, _L("Confirm"), 4, false);
    m_unbind_btn->SetFontUniformColor(wxColour("#ffffff"));
    m_unbind_btn->SetBGColor(wxColour("#419488"));
    m_unbind_btn->SetBGHoverColor(wxColour("#65A79E"));
    m_unbind_btn->SetBGPressColor(wxColour("#1A8676"));
    m_unbind_btn->SetBGDisableColor(wxColour("#dddddd"));
    m_unbind_btn->SetSize(-1, FromDIP(30));
    m_unbind_btn->SetMinSize(wxSize(100, FromDIP(30)));
    m_cancel_btn = new FFButton(this, wxID_ANY, _L("Cancel"), 4, true);
    m_cancel_btn->SetFontColor(wxColour("#333333"));
    m_cancel_btn->SetFontHoverColor(wxColour("#65A79E"));
    m_cancel_btn->SetFontPressColor(wxColour("#1A8676"));
    m_cancel_btn->SetFontDisableColor(wxColour("#dddddd"));    
    m_cancel_btn->SetBorderColor(wxColour("#333333"));
    m_cancel_btn->SetBorderHoverColor(wxColour("#65A79E"));
    m_cancel_btn->SetBorderPressColor(wxColour("#1A8676"));
    m_cancel_btn->SetBorderDisableColor(wxColour("#dddddd"));
    m_cancel_btn->SetBGUniformColor(wxColour("#ffffff"));
    m_cancel_btn->SetSize(-1, FromDIP(30));
    m_cancel_btn->SetMinSize(wxSize(100, FromDIP(30)));

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->AddStretchSpacer(1);
    btnSizer->Add(m_cancel_btn, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    btnSizer->AddSpacer(FromDIP(50));
    btnSizer->Add(m_unbind_btn, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT);
    btnSizer->AddStretchSpacer(1);
    
    main_sizer->AddSpacer(FromDIP(30));
    main_sizer->Add(topSizer, 0, wxEXPAND | wxALIGN_CENTER | wxLEFT | wxRIGHT, FromDIP(30));
    main_sizer->AddSpacer(FromDIP(30));
    main_sizer->Add(m_unbind_text, 0, wxEXPAND | wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT, FromDIP(30));
    main_sizer->AddSpacer(FromDIP(36));
    main_sizer->Add(btnSizer, 0, wxEXPAND | wxALIGN_CENTER_HORIZONTAL);
    main_sizer->AddSpacer(FromDIP(50));
    //main_sizer->Layout();

    //main_sizer->Fit(this);
    //SetSizer(m_sizer_main);
    Layout();
    Fit();
    Centre(wxBOTH);

    #if 0
    Bind(wxEVT_WEBREQUEST_STATE, [this](wxWebRequestEvent& evt) {
         switch (evt.GetState()) {
         case wxWebRequest::State_Completed: {
             BOOST_LOG_TRIVIAL(error) << "BindDialog: web request state completed";
             wxImage avatar_stream = *evt.GetResponse().GetStream();
             if (avatar_stream.IsOk()) {
                 avatar_stream.Rescale(FromDIP(80), FromDIP(80));
                 //auto bitmap = new wxBitmap(avatar_stream);
                 //bitmap->SetSize(wxSize(FromDIP(60), FromDIP(60)));
                 //m_user_img->SetBitmap(*bitmap);
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
    Bind(wxEVT_SHOW, &UnBindMachineDialog::on_show, this);
    Bind(wxEVT_CLOSE_WINDOW, &UnBindMachineDialog::on_close, this);
    m_unbind_btn->Bind(wxEVT_BUTTON, &UnBindMachineDialog::on_unbind_printer, this);
    m_cancel_btn->Bind(wxEVT_BUTTON, &UnBindMachineDialog::on_cancel, this);
    Bind(EVT_UNBIND_MACHINE_COMPLETED, &UnBindMachineDialog::on_unbind_completed, this);

    wxGetApp().UpdateDlgDarkUI(this);
}

UnBindMachineDialog::~UnBindMachineDialog()
{
    Unbind(EVT_UNBIND_MACHINE_COMPLETED, &UnBindMachineDialog::on_unbind_completed, this);
    if (m_unbind_info) {
        delete m_unbind_info;
        m_unbind_info = nullptr;
    }
}

void UnBindMachineDialog::on_cancel(wxCommandEvent &event)
{
    on_destroy();
    EndModal(wxID_CANCEL);
}

void UnBindMachineDialog::on_destroy()
{
    /*
    if (m_unbind_job) {
        m_unbind_job->cancel();
        m_unbind_job->join();
    }*/ //by ymd
    //if (m_web_request.IsOk()) {
    //    m_web_request.Cancel();
    //}
}

    //if (m_web_request.IsOk()) {
    //    m_web_request.Cancel();
    //}
}

void UnBindMachineDialog::downloadUrlPic(const std::string& url) 
{
    if (!url.empty()) {
        Slic3r::Http http   = Slic3r::Http::get(url);
        std::string  suffix = url.substr(url.find_last_of(".") + 1);
        http.header("accept", "image/" + suffix)
            .on_complete([this](std::string body, unsigned int status) {
                wxMemoryInputStream stream(body.data(), body.size());
                wxImage             image(stream, wxBITMAP_TYPE_ANY);
                if (!image.IsOk()) {
                    BOOST_LOG_TRIVIAL(error) << "UnBindMachineDialog download image is not ok";
                    return;
                }
                wxGetApp().setUsrPic(image);
                image.Rescale(FromDIP(80), FromDIP(80));
                m_user_panel->SetImage(image);
                Layout();
            })
            .on_error([=](std::string body, std::string error, unsigned status) {
                BOOST_LOG_TRIVIAL(info) << " UnBindMachineDialog::downloadUrlPic: status:" << status << " error:" << error;
            })
            .perform();
    } else {
        wxImage     tmpimage;
        std::string name = "login_default_usr_pic";
        if (tmpimage.LoadFile(Slic3r::GUI::from_u8(Slic3r::var(name + ".png")), wxBITMAP_TYPE_PNG)) {
            wxGetApp().setUsrPic(tmpimage);
            tmpimage.Rescale(FromDIP(80), FromDIP(80));
            m_user_panel->SetImage(tmpimage);
            Layout();
        }
    }
}

void UnBindMachineDialog::on_result_ok(wxCommandEvent& event)
{
    if (m_result_code != 0) {
        //m_simplebook->SetSelection(0);
        Layout();
        Fit();
    } else {
        on_destroy();
        EndModal(wxID_OK);
    }
}

void UnBindMachineDialog::on_close(wxCloseEvent &event)
{
    on_destroy();
    event.Skip();
}

void UnBindMachineDialog::on_unbind_completed(wxCommandEvent &event)
{
    m_result_code = event.GetInt();
    if (m_result_code == COM_OK) {
        BOOST_LOG_TRIVIAL(info) << "unbind success";
        //m_result_text->SetLabel(_L("The device is registered successfully!"));
        //m_result_text->SetForegroundColour(wxColor("#419488"));
        //m_result_sizer->Layout();
        //m_simplebook->SetSelection(1);
        EndModal(wxID_OK);
    } else {
        //m_result_text->SetLabel(_L("The device is registered fail!"));
        //m_result_text->SetForegroundColour(wxColor("#EA3522"));
        //m_result_sizer->Layout();
        //m_simplebook->SetSelection(1);
        BOOST_LOG_TRIVIAL(error) << "unbind error: " << m_result_code;
        EndModal(wxID_CANCEL);
    }
    Layout();
    Fit();
}

void UnBindMachineDialog::on_unbind_printer(wxCommandEvent &event)
{
    event.Skip();
    m_unbind_btn->Enable(false);
    m_result_code = 0;
    //if (!m_device_info) {
    //    BOOST_LOG_TRIVIAL(error) << "device_info is null"; 
    //    m_unbind_btn->Enable(true);
    //    return;
    //}
    //m_unbind_job = std::make_shared<UnbindJob>(m_device_info);

    
    m_unbind_job = std::make_shared<UnbindJob>(m_unbind_info->dev_id, m_unbind_info->bind_id, m_unbind_info->nim_account_id);
    m_unbind_job->set_event_handle(this);
    m_unbind_job->process();
}

void UnBindMachineDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    m_unbind_btn->SetMinSize(BIND_DIALOG_BUTTON_SIZE);
    m_cancel_btn->SetMinSize(BIND_DIALOG_BUTTON_SIZE);
}

//void UnBindMachineDialog::update_device_info(DeviceObject* info)
//{
//    m_device_info = info;
//}

void UnBindMachineDialog::update_device_info2(BindInfo *info)
{ 
    m_unbind_info = info;
}

void UnBindMachineDialog::update_machine_info(MachineObject *info)
{
    m_machine_info = info;
}

void UnBindMachineDialog::on_show(wxShowEvent &event)
{
    m_result_code   = 0;
    //m_result_extra  = wxEmptyString;
    //m_result_info   = wxEmptyString;

    if (event.IsShown()) {
        wxBitmap bmp;
        //auto pid = m_device_info->get_dev_pid();
        if (0x0024 == m_unbind_info->dev_pid) { // ad 5m pro
            bmp = create_scaled_bitmap("adventurer_5m_pro", 0, 80);
        } else if (0x0023 == m_unbind_info->dev_pid) { // ad 5m
            bmp = create_scaled_bitmap("adventurer_5m", 0, 80);
        }else if(0x001F == m_unbind_info->dev_pid){ //G3U
            bmp = create_scaled_bitmap("guider_3_ultra", 0, 80);
        } else if (0x0026 == m_unbind_info->dev_pid) { // ad5x
            bmp = create_scaled_bitmap("ad5x", 0, 80);
        } else if (0x0025 == m_unbind_info->dev_pid) { // Guider4
            bmp = create_scaled_bitmap("Guider4", 0, 80);
        }
        else {
            auto img_path = m_unbind_info->img /*m_device_info->get_printer_thumbnail_img_str()*/;
            if (wxGetApp().dark_mode()) { img_path += "_dark"; }
            bmp = create_scaled_bitmap(img_path, this, FromDIP(80));
        }
        m_printer_img->SetBitmap(bmp);
        m_printer_img->Refresh();
        //m_printer_img->Show();

        m_printer_name->SetLabelText(from_u8(m_unbind_info->dev_name /*m_device_info->get_dev_name()*/));
        //m_machinePanel->Layout();
        m_machine_sizer->Layout();

        if (LoginDialog::IsUsrLogin()) {
            auto user_info = LoginDialog::GetUsrInfo();
            BOOST_LOG_TRIVIAL(error) << "Get user info: nickname (" << user_info.nickname << "), headImgUrl (" << user_info.headImgUrl << ")";
            wxString username = wxString::FromUTF8(user_info.nickname);
            wxGCDC   dc(this);
            wxString clipName = FFUtils::trimString(dc, username, FromDIP(USER_NAME_LENGTH));
            m_user_name->SetLabelText(clipName);
            m_user_name->SetToolTip(wxString::FromUTF8(user_info.nickname));
            //m_user_name->SetLabelText(wxString::FromUTF8(user_info.nickname));
            #if 0
            if (!user_info.headImgUrl.empty()) {
                m_web_request = wxWebSession::GetDefault().CreateRequest(this, user_info.headImgUrl);
                if (!m_web_request.IsOk()) {
                    BOOST_LOG_TRIVIAL(error) << "web session create request fail";
                } else {
                    m_web_request.Start();
                }
            }
            #else
            #if 1
            wxImage image = wxGetApp().getUsrPic();
            if (image.IsOk()) {
                image.Rescale(FromDIP(80), FromDIP(80));
                m_user_panel->SetImage(image);
                Layout();
            } else {
                downloadUrlPic(user_info.headImgUrl);
            }
            #endif
            #endif
            m_user_sizer->Layout();
        }
        //m_simplebook->Layout();
        GetSizer()->Fit(this);
        Layout();
        Fit();
        event.Skip();
    }
}

} // namespace Slic3r::GUI
