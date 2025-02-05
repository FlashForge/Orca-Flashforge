#include "Tab.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/Model.hpp"

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
    m_staticText_add_machine = new wxStaticText(this, wxID_ANY, wxT("click to add machine"), wxDefaultPosition, wxDefaultSize, 0);
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
    wxGetApp().Bind(EVT_LOGIN_OUT, [this](wxCommandEvent& event) {
        if (m_side_tools) {
            m_side_tools->setAccountState(true);
        }
    });
}

MonitorPanel::~MonitorPanel()
{
    //m_side_tools->get_panel()->Disconnect(wxEVT_LEFT_DOWN, wxMouseEventHandler(MonitorPanel::on_printer_clicked), NULL, this);
    m_side_tools->get_panel()->Disconnect(EVT_DEV_LIST_BTN_CLICKED, wxMouseEventHandler(MonitorPanel::on_printer_clicked), NULL, this);
    delete m_side_tools;
    m_side_tools = nullptr;
    Slic3r::GUI::MultiComMgr::inst()->Unbind(COM_WAN_DEV_MAINTAIN_EVENT, &MonitorPanel::onComWanDevMaintainEvent, this);
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

 void MonitorPanel::init_timer()
{
    m_refresh_timer = new wxTimer();
    m_refresh_timer->SetOwner(this);
    m_refresh_timer->Start(REFRESH_INTERVAL);
    wxPostEvent(this, wxTimerEvent());

    Slic3r::DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;
    MachineObject *obj_ = dev->get_selected_machine();
    if (obj_)
        GUI::wxGetApp().sidebar().load_ams_list(obj_->dev_id, obj_);
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
        page->SetFocus();
    }, m_tabpanel->GetId());

    //m_status_add_machine_panel = new AddMachinePanel(m_tabpanel);
    m_device_list_panel = new DeviceListPanel(m_tabpanel);
    m_tabpanel->AddPage(m_device_list_panel, _L("Device List"), "", true);

    //m_status_info_panel        = new StatusPanel(m_tabpanel);
    //m_tabpanel->AddPage(m_status_info_panel, _L("Device Status"), "", false);
    m_status_info_panel_page   = new SingleDeviceState(m_tabpanel);
    m_tabpanel->AddPage(m_status_info_panel_page, _L("Device Status"), "", false);

    //m_media_file_panel = new MediaFilePanel(m_tabpanel);
    //m_tabpanel->AddPage(m_media_file_panel, _L("SD Card"), "", false);
    //m_tabpanel->AddPage(m_media_file_panel, _L("Internal Storage"), "", false);

    //m_upgrade_panel = new UpgradePanel(m_tabpanel);
    //m_tabpanel->AddPage(m_upgrade_panel, _L("Update"), "", false);

    //m_hms_panel = new HMSPanel(m_tabpanel);
    //m_tabpanel->AddPage(m_hms_panel, _L("HMS"),"", false);

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

    wxGetApp().sidebar().load_ams_list({}, {});
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

void MonitorPanel::on_update_all(wxMouseEvent &event)
{
    update_all();
    Layout();
    Refresh();
}

 void MonitorPanel::on_timer(wxTimerEvent& event)
{
    update_all();

    Layout();
    Refresh();
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

    if (!dev->set_selected_machine(event.GetString().ToStdString()))
        return;*/

    set_default();
    update_all();

    /*DeviceObject *obj_ = devOpr->get_selected_machine();
    if (obj_)
        GUI::wxGetApp().sidebar().load_ams_list(obj_->get_dev_id(), obj_);*/

    Layout();
    Refresh();
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

void MonitorPanel::on_size(wxSizeEvent &event)
{
    Layout();
    Refresh();
}

void MonitorPanel::onComWanDevMaintainEvent(ComWanDevMaintainEvent& event) 
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (event.login && !event.online) {
        if (m_side_tools) {
            m_side_tools->setAccountState(false);
        }
    } else if (event.login && event.online) {
        if (m_side_tools) {
            m_side_tools->setAccountState(true);
        }
    }
    event.Skip();
}

void MonitorPanel::update_all()
{
    DeviceObjectOpr       *devOpr  = wxGetApp().getDeviceObjectOpr();
    if (!devOpr)
        return;
    DeviceObject *obj = devOpr->get_selected_machine();

    // BBS check mqtt connections if user is login
    //if (wxGetApp().is_user_login()) {
    //    dev->check_pushing();
    //    // check mqtt connection and reconnect if disconnected
    //    try {
    //        m_agent->refresh_connection();
    //    } catch (...) {
    //        ;
    //    }
    //}

    if (obj) {
        //wxGetApp().reset_to_active();
        if (obj->connection_type() != last_conn_type) {
            last_conn_type = obj->connection_type();
        }
    }

    // m_status_info_panel->obj = obj;
    // m_status_info_panel->m_media_play_ctrl->SetMachineObject(obj);
    //m_upgrade_panel->update(obj);
    //m_media_file_panel->SetMachineObject(obj);
    //m_side_tools->update_device_status(obj);

    if (!obj && m_side_tools && !m_side_tools->getAccountState()) {
        show_status((int) MONITOR_LOGIN_OFFLINE);
        return;
    }
    if (!obj) {
        show_status((int) MONITOR_NO_PRINTER);
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
        if (!m_side_tools->getAccountState()) {
            show_status(MONITOR_LOGIN_OFFLINE);
        } else {
            show_status(MONITOR_NORMAL);
        }
        return;
    }

    // if (m_status_info_panel->IsShown()) {
    //     m_status_info_panel->update(obj);
    // }

    /*if (m_hms_panel->IsShown()) {
        m_hms_panel->update(obj);
    }

#if !BBL_RELEASE_TO_PUBLIC
    if (m_upgrade_panel->IsShown()) {
        m_upgrade_panel->update(obj);
    }
#endif*/
}

bool MonitorPanel::Show(bool show)
{
#ifdef __APPLE__
    wxGetApp().mainframe->SetMinSize(wxGetApp().plater()->GetMinSize());
#endif

    NetworkAgent* m_agent = wxGetApp().getAgent();
    DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (show) {
        m_refresh_timer->Stop();
        m_refresh_timer->SetOwner(this);
        m_refresh_timer->Start(REFRESH_INTERVAL);
        wxPostEvent(this, wxTimerEvent());

        if (dev) {
            //set a default machine when obj is null
            obj = dev->get_selected_machine();
            if (obj == nullptr) {
                dev->load_last_machine();
                obj = dev->get_selected_machine();
                if (obj) 
                    GUI::wxGetApp().sidebar().load_ams_list(obj->dev_id, obj);
            } else {
                obj->reset_update_time();
            }
        }
    } else {
        m_refresh_timer->Stop();
    }
    return wxPanel::Show(show);
}

void MonitorPanel::update_side_panel()
{
    Slic3r::DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;

    auto is_next_machine = false;
    if (!dev->get_first_online_user_machine().empty()) {
        wxCommandEvent* event = new wxCommandEvent(wxEVT_COMMAND_CHOICE_SELECTED);
        event->SetString(dev->get_first_online_user_machine());
        wxQueueEvent(this, event);
        is_next_machine = true;
        return;
    }

    if (!is_next_machine) { m_side_tools->set_none_printer_mode(); }
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

   
#if !BBL_RELEASE_TO_PUBLIC
    //m_upgrade_panel->update(nullptr);
#endif
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
    //m_status_info_panel->show_status(status);
    //m_hms_panel->show_status(status);
    //m_upgrade_panel->show_status(status);
    //m_media_file_panel->Enable(status == MonitorStatus::MONITOR_NORMAL);

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

} // GUI
} // Slic3r
