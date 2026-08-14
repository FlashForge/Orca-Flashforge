#include "Tab.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/AppConfig.hpp"
#include "slic3r/Utils/bambu_networking.hpp"
#include "slic3r/Utils/NetworkAgent.hpp"

#include <wx/app.h>
#include <wx/button.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>

#include <wx/bmpcbox.h>
#include <wx/bmpbuttn.h>
#include <wx/treectrl.h>
#include <wx/imaglist.h>
#include <wx/settings.h>
#include <wx/filedlg.h>
#include <wx/wupdlock.h>
#include <wx/dataview.h>
#include <wx/tglbtn.h>

#include "wxExtensions.hpp"
#include "GUI_App.hpp"
#include "GUI_ObjectList.hpp"
#include "Plater.hpp"
#include "MainFrame.hpp"
#include "Widgets/Label.hpp"
#include "format.hpp"
#include "MediaPlayCtrl.h"
#include "MediaFilePanel.h"
#include "Plater.hpp"
#include "BindDialog.hpp"
#include "FlashForge/DeviceListPanel.hpp"
#include "FlashForge/DeviceData.hpp"
#include "DeviceCore/DevManager.h"
#include "FlashForge/SingleDeviceState.hpp"

namespace Slic3r {
namespace GUI {

#define REFRESH_INTERVAL       1000

AddMachinePanel::AddMachinePanel(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style, const wxString& name)
    : wxPanel(parent, id, pos, size, style)
{
    this->SetBackgroundColour(0xEEEEEE);

    wxBoxSizer* topsizer = new wxBoxSizer(wxVERTICAL);

    topsizer->AddStretchSpacer();

    m_bitmap_empty = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0);
    m_bitmap_empty->SetBitmap(create_scaled_bitmap("monitor_status_empty", nullptr, 250));
    topsizer->Add(m_bitmap_empty, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 0);
    topsizer->AddSpacer(46);

    wxBoxSizer* horiz_sizer = new wxBoxSizer(wxHORIZONTAL);
    horiz_sizer->Add(0, 0, 538, 0, 0);

    wxBoxSizer* btn_sizer = new wxBoxSizer(wxVERTICAL);
    m_button_add_machine = new Button(this, "", "monitor_add_machine", FromDIP(24));
    m_button_add_machine->SetCornerRadius(FromDIP(12));
    StateColor button_bg(
        std::pair<wxColour, int>(0xCECECE, StateColor::Pressed),
        std::pair<wxColour, int>(0xCECECE, StateColor::Hovered),
        std::pair<wxColour, int>(this->GetBackgroundColour(), StateColor::Normal)
    );
    m_button_add_machine->SetBackgroundColor(button_bg);
    m_button_add_machine->SetBorderColor(0x909090);
    m_button_add_machine->SetMinSize(wxSize(96, 39));
    btn_sizer->Add(m_button_add_machine, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);
    m_staticText_add_machine = new wxStaticText(this, wxID_ANY, _L("click to add machine"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText_add_machine->Wrap(-1);
    m_staticText_add_machine->SetForegroundColour(0x909090);
    btn_sizer->Add(m_staticText_add_machine, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);

    horiz_sizer->Add(btn_sizer);
    horiz_sizer->Add(0, 0, 624, 0, 0);

    topsizer->Add(horiz_sizer, 0, wxEXPAND, 0);

    topsizer->AddStretchSpacer();

    this->SetSizer(topsizer);
    this->Layout();

    // Connect Events
    m_button_add_machine->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(AddMachinePanel::on_add_machine), NULL, this);
}

void AddMachinePanel::msw_rescale() {

}

void AddMachinePanel::on_add_machine(wxCommandEvent& event) {
    // load a url
}

AddMachinePanel::~AddMachinePanel() {
    m_button_add_machine->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(AddMachinePanel::on_add_machine), NULL, this);
}

wxDEFINE_EVENT(EVT_SWITCH_TO_DEVICE_LIST, wxCommandEvent);
wxDEFINE_EVENT(EVT_SWITCH_TO_DEVICE_STATUS, wxCommandEvent);
 MonitorPanel::MonitorPanel(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
    : wxPanel(parent, id, pos, size, style),
     m_select_machine(SelectMachinePopup(this)),
     m_connect_fail_time1(0), 
     m_connect_fail_time0(0)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__

    init_tabpanel();

    m_main_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_main_sizer->Add(m_tabpanel, 1, wxEXPAND | wxLEFT, 0);
    SetSizerAndFit(m_main_sizer);

    init_timer();

    //m_side_tools->get_panel()->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(MonitorPanel::on_printer_clicked), NULL, this);
    m_side_tools->get_panel()->Connect(EVT_DEV_LIST_BTN_CLICKED, wxMouseEventHandler(MonitorPanel::on_printer_clicked), NULL, this);

    Bind(wxEVT_TIMER, &MonitorPanel::on_timer, this);
    Bind(wxEVT_SIZE, &MonitorPanel::on_size, this);
    Bind(wxEVT_COMMAND_CHOICE_SELECTED, &MonitorPanel::on_select_printer, this);

    m_select_machine.Bind(EVT_FINISHED_UPDATE_MACHINE_LIST, [this](wxCommandEvent& e) {
        m_side_tools->start_interval();
    });
#if 0
    Bind(EVT_ALREADY_READ_HMS, [this](wxCommandEvent& e) {
        auto key = e.GetString().ToStdString();
        auto iter = m_hms_panel->temp_hms_list.find(key);
        if (iter != m_hms_panel->temp_hms_list.end()) {
            m_hms_panel->temp_hms_list[key].set_read();
        }

        update_hms_tag();
        e.Skip();
    });

#endif
    Bind(EVT_SWITCH_TO_DEVICE_STATUS, [this](wxCommandEvent& event) {
        m_tabpanel->SetSelection(1);
        m_status_info_panel_page->setCurId(event.GetInt());
        event.Skip();
    });
    Bind(EVT_SWITCH_TO_DEVICE_LIST, [this](wxCommandEvent& event) {
        m_tabpanel->SetSelection(0);
        event.Skip();
    });
    Slic3r::GUI::MultiComMgr::inst()->Bind(COM_WAN_DEV_MAINTAIN_EVENT, &MonitorPanel::onComWanDevMaintainEvent, this);
    wxGetApp().Bind(EVT_LOGIN_OUT, &MonitorPanel::on_login_out, this);
}

MonitorPanel::~MonitorPanel()
{
    //m_side_tools->get_panel()->Disconnect(wxEVT_LEFT_DOWN, wxMouseEventHandler(MonitorPanel::on_printer_clicked), NULL, this);
    m_side_tools->get_panel()->Disconnect(EVT_DEV_LIST_BTN_CLICKED, wxMouseEventHandler(MonitorPanel::on_printer_clicked), NULL, this);
    Slic3r::GUI::MultiComMgr::inst()->Unbind(COM_WAN_DEV_MAINTAIN_EVENT, &MonitorPanel::onComWanDevMaintainEvent, this);
    wxGetApp().Unbind(EVT_LOGIN_OUT, &MonitorPanel::on_login_out, this);
    delete m_side_tools;
    m_side_tools = nullptr;
    if (m_refresh_timer)
        m_refresh_timer->Stop();
    delete m_refresh_timer;
}

void MonitorPanel::OnActivate()
{
    if (0 == m_tabpanel->GetSelection()) {
        m_device_list_panel->OnActivate();
    }
}

void MonitorPanel::init_bitmap()
{
    if (0 == m_tabpanel->GetSelection()) {
        m_device_list_panel->OnActivate();
    }
}

void MonitorPanel::init_timer()
{
    m_refresh_timer = new wxTimer();
    m_refresh_timer->SetOwner(this);
    m_refresh_timer->Start(REFRESH_INTERVAL);
    if (update_flag) { update_all();}
}

void MonitorPanel::init_tabpanel()
{
    m_side_tools = new SideTools(this, wxID_ANY);
    wxBoxSizer* sizer_side_tools = new wxBoxSizer(wxVERTICAL);
    sizer_side_tools->Add(m_side_tools, 1, wxEXPAND, 0);
    m_tabpanel             = new Tabbook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, sizer_side_tools, wxNB_LEFT | wxTAB_TRAVERSAL | wxNB_NOPAGETHEME);
    m_side_tools->set_table_panel(m_tabpanel);
    m_tabpanel->SetBackgroundColour(wxColour("#FEFFFF"));
    m_tabpanel->Bind(wxEVT_BOOKCTRL_PAGE_CHANGED, [this](wxBookCtrlEvent& e) {
        auto page = m_tabpanel->GetCurrentPage();
        //if (page == m_media_file_panel) {
            //auto title = m_tabpanel->GetPageText(m_tabpanel->GetSelection());
            //m_media_file_panel->SwitchStorage(title == _L("SD Card"));
        //}
        if (page == m_status_info_panel_page) {
            m_status_info_panel_page->checkPrinterStatus();
        }
        page->SetFocus();
        update_all();
        }, m_tabpanel->GetId());

    //m_status_add_machine_panel = new AddMachinePanel(m_tabpanel);
    m_device_list_panel = new DeviceListPanel(m_tabpanel);
    m_tabpanel->AddPage(m_device_list_panel, _L("Device List"), "", true);

    //m_status_info_panel        = new StatusPanel(m_tabpanel);
    //m_tabpanel->AddPage(m_status_info_panel, _L("Device Status"), "", false);
    m_status_info_panel_page   = new SingleDeviceState(m_tabpanel);
    m_tabpanel->AddPage(m_status_info_panel_page, _CTX("Device Status", "Flashforge"), "", false);

    //m_media_file_panel = new MediaFilePanel(m_tabpanel);
    //m_tabpanel->AddPage(m_media_file_panel, _L("SD Card"), "", false);
    //m_tabpanel->AddPage(m_media_file_panel, _L("Internal Storage"), "", false);

    //m_upgrade_panel = new UpgradePanel(m_tabpanel);
    //m_tabpanel->AddPage(m_upgrade_panel, _L("Update"), "", false);

    //m_hms_panel = new HMSPanel(m_tabpanel);
    //m_tabpanel->AddPage(m_hms_panel, "HMS","", false);

    //m_hms_panel = new HMSPanel(m_tabpanel);
    //m_tabpanel->AddPage(m_hms_panel, _L("HMS"),"", false);

    m_initialized = true;
    show_status((int)MonitorStatus::MONITOR_NO_PRINTER);
    m_tabpanel->SetSelection(0);
}

void MonitorPanel::set_default()
{
    obj = nullptr;
    last_conn_type = "undefined";

    /* reset status panel*/
    //m_status_info_panel->set_default();

    /* reset side tool*/
    //m_bitmap_wifi_signal->SetBitmap(wxNullBitmap);
}

wxWindow* MonitorPanel::create_side_tools()
{
    //TEST function
    //m_bitmap_wifi_signal->Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(MonitorPanel::on_update_all), NULL, this);

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    auto        panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(0, FromDIP(50)));
    panel->SetBackgroundColour(wxColour(135,206,250));
    panel->SetSizer(sizer);
    sizer->Layout();
    panel->Fit();
    return panel;
}

void MonitorPanel::on_sys_color_changed()
{
    //m_status_info_panel->on_sys_color_changed();
    //m_upgrade_panel->on_sys_color_changed();
    //m_media_file_panel->Rescale();
}

void MonitorPanel::msw_rescale()
{
    /* side_tool rescale */
    m_side_tools->msw_rescale();
    m_tabpanel->Rescale();
    m_status_add_machine_panel->msw_rescale();
    //m_device_list_panel->msw_rescale();
    //m_status_info_panel->msw_rescale();
    //m_status_info_panel_page->msw_rescale();
    //m_media_file_panel->Rescale();
    //m_upgrade_panel->msw_rescale();
    //m_hms_panel->msw_rescale();

    Layout();
    Refresh();
}

void MonitorPanel::select_machine(const std::string &machine_sn)
{
    wxCommandEvent *event = new wxCommandEvent(wxEVT_COMMAND_CHOICE_SELECTED);
    event->SetString(machine_sn);
    wxQueueEvent(this, event);
}


void MonitorPanel::on_timer(wxTimerEvent& event)
{
    if (update_flag) {
        update_all();
        //Layout();
    }
}

void MonitorPanel::on_select_printer(wxCommandEvent &event)
{
    /*DeviceObjectOpr *devOpr = wxGetApp().getDeviceObjectOpr();
    if (!devOpr)
        return;
    if (!devOpr->set_selected_machine(event.GetString().ToStdString()))
        return;

    Slic3r::DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;

    if ( dev->get_selected_machine() && (dev->get_selected_machine()->dev_id != event.GetString().ToStdString()) && m_hms_panel) {
        m_hms_panel->clear_hms_tag();
    }

    if (!dev->set_selected_machine(event.GetString().ToStdString()))
        return;*/

    set_default();
    update_all();

    /*DeviceObject *obj_ = devOpr->get_selected_machine();
    if (obj_)
        GUI::wxGetApp().sidebar().load_ams_list(obj_->get_dev_id(), obj_);*/

    Layout();
}

void MonitorPanel::on_printer_clicked(wxMouseEvent &event)
{
    auto mouse_pos = ClientToScreen(event.GetPosition());
    wxPoint rect = m_side_tools->ClientToScreen(wxPoint(0, 0));
    if (!m_side_tools->is_in_interval()) {
        wxPoint pos      = m_side_tools->ClientToScreen(wxPoint(0, 0));
        wxPoint mainPos  = wxGetApp().mainframe->ClientToScreen(wxPoint(0, 0));
        int     toolPosy = pos.y - mainPos.y;
        pos.y += m_side_tools->GetRect().height;
        // pos.x = pos.x < 0? 0:pos.x;
        m_select_machine.Move(pos);

        wxSize sizeAll = wxGetApp().mainframe->GetSize();   
#ifdef _WIN32
        int side_tools_height = sizeAll.y /*- rect.y*/ - toolPosy - m_side_tools->GetRect().height - m_side_tools->getConnectInfoHeight() - 10;
#else
        int side_tools_height = sizeAll.y /*- rect.y*/ - toolPosy - m_side_tools->GetRect().height - m_side_tools->getConnectInfoHeight() - 40;
#endif
//#ifdef __linux__
        m_select_machine.SetSize(wxSize(m_side_tools->GetSize().x, side_tools_height));
        m_select_machine.SetMaxSize(wxSize(m_side_tools->GetSize().x, side_tools_height));
        m_select_machine.SetMinSize(wxSize(m_side_tools->GetSize().x, side_tools_height));
        //#endif

        m_select_machine.Popup();
    }
}

void MonitorPanel::on_size(wxSizeEvent& event) { Layout(); }

void MonitorPanel::onComWanDevMaintainEvent(ComWanDevMaintainEvent& event) 
{
    if (event.sessionGeneration != 0
        && event.sessionGeneration != MultiComMgr::inst()->wanSessionGeneration()) {
        event.Skip();
        return;
    }
    if (!event.login || event.serviceState == WanServiceState::LoggedOut) {
        m_wan_service_state = WanServiceState::LoggedOut;
        m_connect_fail_time0 = 0;
        m_connect_fail_time1 = 0;
    } else if (event.serviceState == WanServiceState::Connected) {
        m_wan_service_state = WanServiceState::Connected;
    } else {
        m_wan_service_state = event.serviceState == WanServiceState::ApiUnavailable
            ? WanServiceState::ApiUnavailable
            : WanServiceState::Reconnecting;
    }
    update_all();
    // GUI_App owns the global account transition. Monitor only updates its
    // local state and must not consume this shared command event.
    event.Skip();
}

void MonitorPanel::on_login_out(wxCommandEvent& event)
{
    m_wan_service_state = WanServiceState::LoggedOut;
    m_connect_fail_time0 = 0;
    m_connect_fail_time1 = 0;
    update_all();
    event.Skip();
}

void MonitorPanel::update_all()
{
    DeviceObjectOpr       *devOpr  = wxGetApp().getDeviceObjectOpr();
    if (!devOpr)
        return;
    DeviceObject *obj = devOpr->get_selected_machine();
#if 0
    //BBS check mqtt connections if user is login
    if (wxGetApp().is_user_login()) {
        dev->check_pushing();
        // check mqtt connection and reconnect if disconnected
        try {
            m_agent->refresh_connection();
        } catch (...) {
            ;
        }
    }
#endif
    if (obj) {
        //wxGetApp().reset_to_active();
        if (obj->connection_type() != last_conn_type) {
            last_conn_type = obj->connection_type();
        }
    }

#if 0
    m_status_info_panel->obj = obj;
    m_upgrade_panel->update(obj);
    m_status_info_panel->m_media_play_ctrl->SetMachineObject(obj);
    m_media_file_panel->SetMachineObject(obj);
    m_side_tools->update_status(obj);
#endif

    const bool account_reconnecting = m_wan_service_state == WanServiceState::Reconnecting
        || m_wan_service_state == WanServiceState::ApiUnavailable;
    const bool selected_cloud_device = obj && obj->connection_type() == "cloud";

    // A server-side logout may be delivered before the WAN connection exit
    // removes the selected cloud device. Do not present that stale object as a
    // reconnecting account. LAN devices remain available without an account.
    if (m_wan_service_state == WanServiceState::LoggedOut && selected_cloud_device) {
        m_side_tools->update_device_status(nullptr);
        show_status(MONITOR_NO_PRINTER);
        return;
    }
    if (account_reconnecting && selected_cloud_device) {
        m_side_tools->update_device_status(nullptr);
        show_status(MONITOR_ACCOUNT_RECONNECTING);
        return;
    }
    if (!obj) {
        show_status(account_reconnecting ? MONITOR_ACCOUNT_RECONNECTING : MONITOR_NO_PRINTER);
#if 0
        m_hms_panel->clear_hms_tag();
        m_tabpanel->GetBtnsListCtrl()->showNewTag(3, false);
#endif
        return;
    }
    if (m_connect_fail_time0 > 0) {
        time_t connect_failed_time2 = time(nullptr);
        if ((connect_failed_time2 - m_connect_fail_time0) /*/ (double)CLOCKS_PER_SEC*/ > 10) {
            m_connect_fail_time0 = 0;
            obj->set_connecting(false);
        }
    }

    if (m_connect_fail_time1 > 0) {
        time_t connect_failed_time2 = time(nullptr);
        if ((connect_failed_time2 - m_connect_fail_time1) /*/ (double)CLOCKS_PER_SEC*/ > 5) {
            m_connect_fail_time1 = 0;
            obj->set_connected_ready(true);
        }
    }

    if (obj->is_connecting()) {
        if (m_connect_fail_time0 == 0)
            m_connect_fail_time0 = time(nullptr);
        m_side_tools->update_device_status(obj);
        show_status(MONITOR_CONNECTING);
        return;
    } else if (!obj->is_connected_ready()) {  // connect failed
        m_connect_fail_time0 = 0;
        if (m_connect_fail_time1 == 0)
            m_connect_fail_time1 = time(nullptr);
        m_side_tools->update_device_status(obj);
        show_status(MONITOR_CONNECTED_FAILED);
        return;
    }else {
        m_connect_fail_time0 = 0;
        m_side_tools->update_device_status(nullptr);
        show_status(MONITOR_NORMAL);
        return;
    }

#if 0
    if (m_status_info_panel->IsShown()) {
        m_status_info_panel->update(obj);
    }

    if (m_status_info_panel->IsShown()) {
        m_status_info_panel->update(obj);
    }

    if (m_hms_panel->IsShown() ||  (obj->hms_list.size() != m_hms_panel->temp_hms_list.size())) {
        m_hms_panel->update(obj);
    }

#if !BBL_RELEASE_TO_PUBLIC
    if (m_upgrade_panel->IsShown()) {
        m_upgrade_panel->update(obj);
    }
#endif

    update_hms_tag();
#endif
}

void MonitorPanel::update_hms_tag()
{
#if 0
    for (auto hmsitem : m_hms_panel->temp_hms_list) {

        if (!obj) { break;}

        const wxString &msg = wxGetApp().get_hms_query()->query_hms_msg(obj->get_dev_id(), hmsitem.second.get_long_error_code());
        if (msg.empty()){ continue;} /*STUDIO-10363 it's hidden message*/

        if (!hmsitem.second.has_read()) {
            //show HMS new tag
            m_tabpanel->GetBtnsListCtrl()->showNewTag(3, true);
            return;
        }
    }

    m_tabpanel->GetBtnsListCtrl()->showNewTag(3, false);
#endif
}

bool MonitorPanel::Show(bool show)
{
#ifdef __APPLE__
    wxGetApp().mainframe->SetMinSize(wxGetApp().plater()->GetMinSize());
#endif

    NetworkAgent* m_agent = wxGetApp().getAgent();
    DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (show) {
        start_update();
        //update_network_version_footer();

        m_refresh_timer->Stop();
        m_refresh_timer->SetOwner(this);
        m_refresh_timer->Start(REFRESH_INTERVAL);
        if (update_flag) { update_all(); }

        if (dev) {
            //set a default machine when obj is null
            obj = dev->get_selected_machine();
            if (obj == nullptr) {
                dev->load_last_machine();
            } else {
                obj->reset_update_time();
            }
        }
    } else {
        stop_update();
        m_refresh_timer->Stop();
    }
    return wxPanel::Show(show);
}

void MonitorPanel::show_status(int status)
{
    if (!m_initialized) return;
    if (last_status == status)return;
    /*if (last_status & (int)MonitorStatus::MONITOR_CONNECTING != 0) {
        NetworkAgent* agent = wxGetApp().getAgent();
        json j;
        j["dev_id"] = obj ? obj->dev_id : "obj_nullptr";
        if ((status & (int)MonitorStatus::MONITOR_DISCONNECTED) != 0) {
            j["result"] = "failed";
        }
        else if ((status & (int)MonitorStatus::MONITOR_NORMAL) != 0) {
            j["result"] = "success";
        }
    }*/
    last_status = status;

    BOOST_LOG_TRIVIAL(info) << "monitor: show_status = " << status;

    //Freeze();
    // update panels
    if (m_side_tools) {
        int  h                = m_side_tools->getConnectInfoHeight();
        bool connectInfoEmpty = h == 0;
        m_side_tools->show_status(status);
        int  h1                = m_side_tools->getConnectInfoHeight();
        bool connectInfoEmpty1 = h1 == 0;
        wxSize listSize          = m_select_machine.GetSize();
        if (connectInfoEmpty && !connectInfoEmpty1) {
            m_select_machine.SetSize(wxSize(listSize.x, listSize.y - h1));
            m_select_machine.SetMaxSize(wxSize(listSize.x, listSize.y - h1));
            m_select_machine.SetMinSize(wxSize(listSize.x, listSize.y - h1));
        } else if (!connectInfoEmpty && connectInfoEmpty1) {
            m_select_machine.SetSize(wxSize(listSize.x, listSize.y + h));
            m_select_machine.SetMaxSize(wxSize(listSize.x, listSize.y + h));
            m_select_machine.SetMinSize(wxSize(listSize.x, listSize.y + h));
        }
        
    };
#if 0
    m_status_info_panel->show_status(status);
    m_hms_panel->show_status(status);
    m_upgrade_panel->show_status(status);
    m_media_file_panel->Enable(status == MonitorStatus::MONITOR_NORMAL);
#endif

    if (m_select_machine.IsShown()) {
        wxPoint pos = m_side_tools->ClientToScreen(wxPoint(0, 0));
        pos.y += m_side_tools->GetRect().height;
        // pos.x = pos.x < 0? 0:pos.x;
        m_select_machine.Move(pos);
    }

    if ((status & (int)MonitorStatus::MONITOR_NO_PRINTER) != 0) {
        set_default();
        m_tabpanel->Layout();
    } else if (((status & (int)MonitorStatus::MONITOR_NORMAL) != 0) 
        || ((status & (int)MonitorStatus::MONITOR_DISCONNECTED) != 0) 
        || ((status & (int) MonitorStatus::MONITOR_DISCONNECTED_SERVER) != 0) 
        || ((status & (int)MonitorStatus::MONITOR_CONNECTING) != 0)
        || ((status & (int)MonitorStatus::MONITOR_CONNECTED_FAILED) != 0)) 
    {

        if (((status & (int) MonitorStatus::MONITOR_DISCONNECTED) != 0) 
            || ((status & (int) MonitorStatus::MONITOR_DISCONNECTED_SERVER) != 0) 
            || ((status & (int)MonitorStatus::MONITOR_CONNECTING) != 0)
            || ((status & (int)MonitorStatus::MONITOR_CONNECTED_FAILED) != 0)) 
        {
            set_default();
        }
        m_tabpanel->Layout();
    }
    Layout();
    //Thaw();
}

std::string MonitorPanel::get_string_from_tab(PrinterTab tab)
{
    switch (tab) {
    case PT_STATUS :
        return "status";
    case PT_MEDIA:
        return "sd_card";
    case PT_UPDATE:
        return "update";
    case PT_HMS:
        return "HMS";
    case PT_DEBUG:
        return "debug";
    default:
        return "";
    }
    return "";
}

void MonitorPanel::jump_to_HMS()
{
#if 0
    if (!this->IsShown())
        return;
    auto page = m_tabpanel->GetCurrentPage();
    if (page && page != m_hms_panel)
        m_tabpanel->SetSelection(PT_HMS);
#endif
}

void MonitorPanel::jump_to_LiveView()
{
    if (!this->IsShown()) { return; }

    auto page = m_tabpanel->GetCurrentPage();
    if (page && page != m_hms_panel)
    {
        m_tabpanel->SetSelection(PT_STATUS);
    }

    m_status_info_panel->get_media_play_ctrl()->jump_to_play();
}

void MonitorPanel::update_network_version_footer()
{
    std::string binary_version = Slic3r::NetworkAgent::get_version();
    if (binary_version.empty())
        return;

    std::string configured_version = wxGetApp().app_config->get_network_plugin_version();
    std::string suffix = extract_suffix(configured_version);
    std::string configured_base = extract_base_version(configured_version);

    wxString footer_text;
    if (!suffix.empty() && configured_base == binary_version) {
        footer_text = wxString::Format(_L("Network plug-in v%s (%s)"), binary_version, suffix);
    } else {
        footer_text = wxString::Format(_L("Network plug-in v%s"), binary_version);
    }

    m_tabpanel->SetFooterText(footer_text);
}

} // GUI
} // Slic3r
