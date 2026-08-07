#include "ConnectPrinter.hpp"
#include "GUI_App.hpp"
#include <wx/dcgraph.h>
#include <slic3r/GUI/I18N.hpp>
#include <slic3r/GUI/Widgets/Label.hpp>
#include "slic3r/GUI/Widgets/FFButton.hpp"
#include "libslic3r/AppConfig.hpp"
#include "FlashForge/DeviceData.hpp"
#include "FlashForge/MultiComMgr.hpp"
#include "DeviceCore/DevManager.h"

namespace Slic3r { namespace GUI {
#if 0
ConnectPrinterDialog::ConnectPrinterDialog(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &pos, const wxSize &size, long style)
    : DPIDialog(parent, id, _L("ConnectPrinter (LAN)"), pos, size, style)
{
    SetBackgroundColour(*wxWHITE);
    this->SetSizeHints(wxDefaultSize, wxDefaultSize);

    wxBoxSizer *main_sizer;
    main_sizer = new wxBoxSizer(wxHORIZONTAL);

    main_sizer->Add(FromDIP(40), 0);

    wxBoxSizer *sizer_top;
    sizer_top = new wxBoxSizer(wxVERTICAL);

    sizer_top->Add(0, FromDIP(40));

    m_staticText_connection_code = new wxStaticText(this, wxID_ANY, _L("Please input the printer access code:"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText_connection_code->SetFont(Label::Body_15);
    m_staticText_connection_code->SetForegroundColour(wxColour(50, 58, 61));
    m_staticText_connection_code->Wrap(-1);
    sizer_top->Add(m_staticText_connection_code, 0, wxALL, 0);

    sizer_top->Add(0, FromDIP(10));
	
    wxBoxSizer *sizer_connect;
    sizer_connect = new wxBoxSizer(wxHORIZONTAL);

    StateColor btn_bd(std::pair<wxColour, int>(wxColour(50, 141, 251), StateColor::Normal));

    m_textCtrl_code = new TextInput(this, wxEmptyString);
    m_textCtrl_code->GetTextCtrl()->SetMaxLength(10);
    m_textCtrl_code->SetFont(Label::Body_14);
    m_textCtrl_code->SetBorderColor(btn_bd);
    m_textCtrl_code->SetCornerRadius(FromDIP(5));
    m_textCtrl_code->SetSize(wxSize(FromDIP(330), FromDIP(40)));
    m_textCtrl_code->SetMinSize(wxSize(FromDIP(330), FromDIP(40)));
    m_textCtrl_code->GetTextCtrl()->SetSize(wxSize(-1, FromDIP(22)));
    m_textCtrl_code->GetTextCtrl()->SetMinSize(wxSize(-1, FromDIP(22)));
    m_textCtrl_code->SetBackgroundColour(*wxWHITE);
    m_textCtrl_code->GetTextCtrl()->SetForegroundColour(wxColour(107, 107, 107));
    sizer_connect->Add(m_textCtrl_code, 0, wxALL | wxALIGN_CENTER_VERTICAL, 0);

    sizer_connect->Add(FromDIP(20), 0);

    m_button_confirm = new Button(this, _L(""), "access_code_confirm");

    StateColor btn_text(std::pair<wxColour, int>(wxColour(255, 255, 255), StateColor::Normal));
    m_button_confirm->SetBorderColor(btn_text);

    sizer_connect->Add(m_button_confirm, 0, wxALL | wxALIGN_CENTER_VERTICAL, 0);
    
    sizer_connect->Add(FromDIP(60), 0);

    sizer_top->Add(sizer_connect);

    sizer_top->Add(0, FromDIP(35));

    m_staticText_hints = new wxStaticText(this, wxID_ANY, _L("You can find it in \"Settings > Network > Network Mode\"\non the printer; To facilitate device identification, \nyou can change the device name on the device page."), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText_hints->SetFont(Label::Body_15);
    m_staticText_hints->SetForegroundColour(wxColour(50, 58, 61));
    m_staticText_hints->Wrap(-1);
    sizer_top->Add(m_staticText_hints, 0, wxALL, 0);

    sizer_top->Add(0, FromDIP(15));

    wxBoxSizer* error_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_error_panel           = new wxPanel(this, wxID_ANY);
    m_error_panel->SetBackgroundColour(wxColour(250, 207, 202));
    m_error_panel->SetMinSize(wxSize(FromDIP(330), FromDIP(32)));

    wxString error_text = _L("The access code is wrong, please input again.");
    m_label_error_info = new Label(m_error_panel, error_text);
    m_label_error_info->SetFont(Label::Body_15);
    m_label_error_info->SetBackgroundColour(wxColour(250, 207, 202));
    m_label_error_info->SetForegroundColour(wxColour(234, 53, 34));
    
    wxGCDC dc(this);
    int   sw = dc.GetTextExtent(error_text).x;
    if (FromDIP(sw) > FromDIP(320)) {
        m_label_error_info->Wrap(FromDIP(330));
        m_error_panel->SetMinSize(wxSize(FromDIP(330), FromDIP(64)));
    }
    error_sizer->AddStretchSpacer(1);
    error_sizer->Add(m_label_error_info, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL, 0);
    error_sizer->AddStretchSpacer(1);
    m_error_panel->SetSizer(error_sizer);
    error_sizer->Fit(m_error_panel);

    sizer_top->Add(m_error_panel, 0, wxALL, 0);
    //sizer_top->Add(m_label_error_info, 0, wxALL, 0);
    //m_label_error_info->Show(err_hint);
    m_error_panel->Show(err_hint);

    sizer_top->Add(0, FromDIP(40), 0, wxEXPAND, 0);

    main_sizer->Add(sizer_top);

    this->SetSizer(main_sizer);
    this->Layout();
    this->Fit();
    CentreOnParent();

    m_textCtrl_code->Bind(wxEVT_TEXT, &ConnectPrinterDialog::on_input_enter, this);
    m_button_confirm->Bind(wxEVT_BUTTON, &ConnectPrinterDialog::on_button_confirm, this);
    wxGetApp().UpdateDlgDarkUI(this);
}
#endif
ConnectPrinterDialog::ConnectPrinterDialog(bool err_hint /*= false*/) 
    : TitleDialog(static_cast<wxWindow *>(wxGetApp().GetMainTopWindow()), _L("Connect Printer (LAN)"), 6)
{
    SetBackgroundColour(*wxWHITE);
    this->SetSizeHints(wxDefaultSize, wxDefaultSize);

    wxBoxSizer *main_sizer;
    main_sizer = MainSizer(); // new wxBoxSizer(wxHORIZONTAL);

    //main_sizer->Add(FromDIP(40), 0);

    wxBoxSizer *sizer_horizontal;
    sizer_horizontal = new wxBoxSizer(wxHORIZONTAL);
    sizer_horizontal->AddSpacer(FromDIP(50));

    wxBoxSizer *sizer_top;
    sizer_top = new wxBoxSizer(wxVERTICAL);

    sizer_top->Add(0, FromDIP(40));

    m_staticText_connection_code = new wxStaticText(this, wxID_ANY, _L("Please input the printer access code:"), wxDefaultPosition,
                                                    wxDefaultSize, 0);
    m_staticText_connection_code->SetFont(Label::Body_15);
    m_staticText_connection_code->SetForegroundColour(wxColour(50, 58, 61));
    m_staticText_connection_code->Wrap(-1);
    sizer_top->Add(m_staticText_connection_code, 0, wxALL, 0);

    sizer_top->Add(0, FromDIP(10));

    wxBoxSizer *sizer_connect;
    sizer_connect = new wxBoxSizer(wxHORIZONTAL);

    StateColor btn_bd(std::pair<wxColour, int>(wxColour(50, 141, 251), StateColor::Normal));

    m_textCtrl_code = new TextInput(this, wxEmptyString);
    m_textCtrl_code->GetTextCtrl()->SetMaxLength(10);
    m_textCtrl_code->SetFont(Label::Body_14);
    m_textCtrl_code->SetBorderColor(btn_bd);
    m_textCtrl_code->SetCornerRadius(FromDIP(5));
    m_textCtrl_code->SetSize(wxSize(FromDIP(330), FromDIP(40)));
    m_textCtrl_code->SetMinSize(wxSize(FromDIP(330), FromDIP(40)));
    m_textCtrl_code->GetTextCtrl()->SetSize(wxSize(-1, FromDIP(22)));
    m_textCtrl_code->GetTextCtrl()->SetMinSize(wxSize(-1, FromDIP(22)));
    m_textCtrl_code->SetBackgroundColour(*wxWHITE);
    m_textCtrl_code->GetTextCtrl()->SetForegroundColour(wxColour(107, 107, 107));
    sizer_connect->Add(m_textCtrl_code, 0, wxALL | wxALIGN_CENTER_VERTICAL, 0);

    sizer_connect->Add(FromDIP(20), 0);

    m_button_confirm = new FFPushButton(this, wxID_ANY, "access_code_confirm_normal", "access_code_confirm_hover", "access_code_confirm_press", "access_code_confirm_normal",30);

    m_button_confirm->SetBackgroundColour(wxColour(255, 255, 255));
    //StateColor btn_text(std::pair<wxColour, int>(wxColour(255, 255, 255), StateColor::Normal));
    //m_button_confirm->SetBorderColor(btn_text);
    m_button_confirm->SetMinSize(wxSize(FromDIP(30),FromDIP(30)));

    sizer_connect->Add(m_button_confirm, 0, wxALL | wxALIGN_CENTER_VERTICAL, 0);

    sizer_connect->Add(FromDIP(60), 0);

    sizer_top->Add(sizer_connect);

    sizer_top->Add(0, FromDIP(10));

    m_staticText_hints = new wxStaticText(this, wxID_ANY,
                                          _L("You can find it in \"Settings > Network > Network Mode\"\non the printer; To facilitate "
                                             "device identification, \nyou can change the device name on the device page."),
                                          wxDefaultPosition, wxDefaultSize, 0);
    m_staticText_hints->SetFont(Label::Body_15);
    m_staticText_hints->SetForegroundColour(wxColour(50, 58, 61));
    m_staticText_hints->Wrap(-1);
    sizer_top->Add(m_staticText_hints, 0, wxALL, 0);

    sizer_top->Add(0, FromDIP(15));

    wxBoxSizer *error_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_error_panel           = new wxPanel(this, wxID_ANY);
    m_error_panel->SetBackgroundColour(wxColour(250, 207, 202));
    m_error_panel->SetMinSize(wxSize(FromDIP(330), FromDIP(32)));

    wxString error_text = _L("The access code is wrong, please input again.");
    m_label_error_info  = new Label(m_error_panel, error_text);
    m_label_error_info->SetFont(Label::Body_15);
    m_label_error_info->SetBackgroundColour(wxColour(250, 207, 202));
    m_label_error_info->SetForegroundColour(wxColour(234, 53, 34));

    wxGCDC dc(this);
    int    sw = dc.GetTextExtent(error_text).x;
    if (FromDIP(sw) > FromDIP(320)) {
        m_label_error_info->Wrap(FromDIP(330));
        m_error_panel->SetMinSize(wxSize(FromDIP(330), FromDIP(56)));
    }
    error_sizer->AddStretchSpacer(1);
    error_sizer->Add(m_label_error_info, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL, 0);
    error_sizer->AddStretchSpacer(1);
    m_error_panel->SetSizer(error_sizer);
    error_sizer->Fit(m_error_panel);

    sizer_top->Add(m_error_panel, 0, wxALL, 0);
    // sizer_top->Add(m_label_error_info, 0, wxALL, 0);
    // m_label_error_info->Show(err_hint);
    m_error_panel->Show(err_hint);

    sizer_top->Add(0, FromDIP(40), 0, wxEXPAND, 0);


    sizer_horizontal->Add(sizer_top);
    //sizer_horizontal->AddSpacer(FromDIP(50));
    main_sizer->Add(sizer_horizontal, 0, wxCENTER);


    //this->SetSizer(main_sizer);
    this->Layout();
    this->Fit();
    CentreOnParent();
    Bind(wxEVT_SHOW, &ConnectPrinterDialog::on_show, this);
    m_textCtrl_code->Bind(wxEVT_TEXT, &ConnectPrinterDialog::on_input_enter, this);
    m_button_confirm->Bind(wxEVT_LEFT_DOWN, &ConnectPrinterDialog::on_button_confirm, this);
    MultiComMgr::inst()->Bind(COM_CONNECTION_READY_EVENT, &ConnectPrinterDialog::on_connect_ready, this);
    MultiComMgr::inst()->Bind(COM_CONNECTION_EXIT_EVENT, &ConnectPrinterDialog::on_connect_exit, this);
    wxGetApp().UpdateDlgDarkUI(this);
}

ConnectPrinterDialog::~ConnectPrinterDialog()
{
    MultiComMgr::inst()->Unbind(COM_CONNECTION_READY_EVENT, &ConnectPrinterDialog::on_connect_ready, this);
    MultiComMgr::inst()->Unbind(COM_CONNECTION_EXIT_EVENT, &ConnectPrinterDialog::on_connect_exit, this);
}

void ConnectPrinterDialog::on_show(wxShowEvent &event) 
{ 
    Layout(); 
}

void ConnectPrinterDialog::end_modal(wxStandardID id)
{
    EndModal(id);
}

void ConnectPrinterDialog::set_machine_object(MachineObject* obj) 
{ 
    m_obj = obj;
}

void ConnectPrinterDialog::set_device_object(DeviceObject* devObj)
{
    m_devObj = devObj;
    if (m_devObj != nullptr) {
        m_dev_id = m_devObj->get_dev_id();
    }
}

void ConnectPrinterDialog::set_connecting_state(bool connecting)
{
    m_connecting = connecting;
    if (m_textCtrl_code != nullptr) {
        m_textCtrl_code->Enable(!connecting);
    }
    if (m_button_confirm != nullptr) {
        m_button_confirm->Enable(!connecting);
    }
}

void ConnectPrinterDialog::show_status(const wxString& text, bool error)
{
    if (m_label_error_info == nullptr || m_error_panel == nullptr) {
        return;
    }

    const wxColour bg = error ? wxColour(250, 207, 202) : wxColour(230, 242, 255);
    const wxColour fg = error ? wxColour(234, 53, 34) : wxColour(50, 58, 61);
    m_error_panel->SetBackgroundColour(bg);
    m_label_error_info->SetBackgroundColour(bg);
    m_label_error_info->SetForegroundColour(fg);
    m_label_error_info->SetLabel(text);
    m_label_error_info->Wrap(FromDIP(330));
    m_error_panel->Show(true);
    Layout();
    Fit();
}

void ConnectPrinterDialog::on_input_enter(wxCommandEvent& evt)
{
    m_input_access_code = evt.GetString();
}


void ConnectPrinterDialog::on_button_confirm(wxMouseEvent &event)
{
    if (m_connecting) {
        return;
    }

    wxString code = m_textCtrl_code->GetTextCtrl()->GetValue();
    if (code.empty()) {
        show_status(_L("Please input the printer access code."), true);
        return;
    }
    for (char c : code) {
        if (!('0' <= c && c <= '9' || 'a' <= c && c <= 'z' || 'A' <= c && c <= 'Z')) {
            show_status(_L("Invalid input."), true);
            return;
        }
    }

    DeviceObjectOpr* dev_opr = wxGetApp().getDeviceObjectOpr();
    if (m_dev_id.empty() && m_devObj != nullptr) {
        m_dev_id = m_devObj->get_dev_id();
    }
    if (m_dev_id.empty() || dev_opr == nullptr || !dev_opr->set_device_access_code(m_dev_id, code.ToStdString(), false)) {
        show_status(_L("Failed to connect to printer."), true);
        return;
    }

    if (m_need_connect) {
        show_status(_L("Connecting, please wait..."), false);
        set_connecting_state(true);
        m_pending_conn_id = ComInvalidId;
        dev_opr->set_selected_machine(m_dev_id);
        m_pending_conn_id = dev_opr->get_connection_id(m_dev_id);
        if (m_pending_conn_id == ComInvalidId) {
            set_connecting_state(false);
            show_status(_L("Failed to connect to printer."), true);
            return;
        }
    } else {
        EndModal(wxID_OK);
    }
    event.Skip();
}

void ConnectPrinterDialog::on_connect_ready(ComConnectionReadyEvent& event)
{
    event.Skip();
    if (event.id != m_pending_conn_id) {
        return;
    }

    m_pending_conn_id = ComInvalidId;
    set_connecting_state(false);
    EndModal(wxID_OK);
}

void ConnectPrinterDialog::on_connect_exit(ComConnectionExitEvent& event)
{
    event.Skip();
    if (event.id != m_pending_conn_id) {
        return;
    }

    m_pending_conn_id = ComInvalidId;
    set_connecting_state(false);

    switch (event.ret) {
    case COM_VERIFY_LAN_DEV_FAILED:
        show_status(_L("The access code is wrong, please input again."), true);
        break;
    case COM_CONN_SEND_ERROR:
    case COM_ERROR:
        show_status(_L("Failed to connect to printer. Please check the printer and network, then try again."), true);
        break;
    default:
        show_status(_L("Failed to connect to printer. Please try again."), true);
        break;
    }
}


void ConnectPrinterDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    m_textCtrl_code->GetTextCtrl()->SetSize(wxSize(-1, FromDIP(22)));
    m_textCtrl_code->GetTextCtrl()->SetMinSize(wxSize(-1, FromDIP(22)));

    //m_button_confirm->SetCornerRadius(FromDIP(12));
    //m_button_confirm->Rescale();
    
    Layout();
    this->Refresh();
}

}} // namespace Slic3r::GUI
