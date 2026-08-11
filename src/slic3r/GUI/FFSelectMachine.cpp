#include "FFSelectMachine.hpp"
#include "I18N.hpp"

#include "libslic3r/Utils.hpp"
#include "libslic3r/Thread.hpp"
#include "GUI.hpp"
#include "GUI_App.hpp"
#include "GUI_Preview.hpp"
#include "MainFrame.hpp"
#include "format.hpp"
#include "Widgets/ProgressDialog.hpp"
#include "Widgets/RoundedRectangle.hpp"
#include "Widgets/StaticBox.hpp"
#include "ConnectPrinter.hpp"
#include "Jobs/BoostThreadWorker.hpp"
#include "Jobs/PlaterWorker.hpp"
#include "FlashForge/MultiComMgr.hpp"
#include "FlashForge/LoginDialog.hpp"
#include "FlashForge/DeviceData.hpp"

#include <wx/progdlg.h>
#include <wx/clipbrd.h>
#include <wx/dcgraph.h>
#include <miniz.h>
#include <algorithm>
#include "Plater.hpp"
#include "Notebook.hpp"
#include "BitmapCache.hpp"
#include "BindDialog.hpp"

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(EVT_UPDATE_WINDOWS_POSITION, wxCommandEvent);
wxDEFINE_EVENT(EVT_FINISHED_UPDATE_MACHINE_LIST, wxCommandEvent);
wxDEFINE_EVENT(EVT_UPDATE_USER_MACHINE_LIST, wxCommandEvent);
wxDEFINE_EVENT(EVT_PRINT_JOB_CANCEL, wxCommandEvent);
wxDEFINE_EVENT(EVT_BIND_MACHINE, wxCommandEvent);
wxDEFINE_EVENT(EVT_UNBIND_MACHINE, wxCommandEvent);
wxDEFINE_EVENT(EVT_DISSMISS_MACHINE_LIST, wxCommandEvent);
wxDEFINE_EVENT(EVT_CONNECT_LAN_PRINT, wxCommandEvent);
wxDEFINE_EVENT(EVT_EDIT_PRINT_NAME, wxCommandEvent);
wxDEFINE_EVENT(EVT_CLEAR_IPADDRESS, wxCommandEvent);

#define INITIAL_NUMBER_OF_MACHINES 0
#define LIST_REFRESH_INTERVAL 200
#define MACHINE_LIST_REFRESH_INTERVAL 5000

#define WRAP_GAP FromDIP(10)

static wxString task_canceled_text = _L("Task canceled");

namespace {

wxColour select_machine_light_text(const wxColour& color)
{
#ifdef __APPLE__
    return color;
#else
    return StateColor::darkModeColorFor(color);
#endif
}

void apply_select_machine_title_style(wxStaticText* text)
{
#ifdef __APPLE__
    if (!text) {
        return;
    }
    text->SetForegroundColour(SELECT_MACHINE_GREY900);
#endif
}

} // namespace

#ifdef __APPLE__
bool SelectMachinePopup::m_wan_bind_enable = false;
#endif

MachineObjectPanel::MachineObjectPanel(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size, long style, const wxString &name)
{
    wxPanel::Create(parent, id, pos, SELECT_MACHINE_ITEM_SIZE, style, name);
    Bind(wxEVT_PAINT, &MachineObjectPanel::OnPaint, this);

    m_info = nullptr;
    m_devInfo = nullptr;

    SetBackgroundColour(wxColour("#fafafa") /*StateColor::darkModeColorFor(*wxWHITE)*/);

    //m_unbind_img        = ScalableBitmap(this, "unbind", 18);
    m_edit_name_img     = ScalableBitmap(this, "edit_button", 18);
    //m_select_unbind_img = ScalableBitmap(this, "unbind_select", 18);

    m_printer_status_offline_lan = ScalableBitmap(this, "printer_status_offline_lan", 16);
    m_printer_status_offline_wan = ScalableBitmap(this, "printer_status_offline_wan", 16);
    m_printer_status_busy    = ScalableBitmap(this, "printer_status_busy", 12);
    m_printer_status_idle    = ScalableBitmap(this, "printer_status_idle", 12);
    m_printer_online_lan         = ScalableBitmap(this, "printer_online_lan", 16);
    m_printer_online_wan         = ScalableBitmap(this, "printer_online_wan", 16);

    this->Bind(wxEVT_ENTER_WINDOW, &MachineObjectPanel::on_mouse_enter, this);
    this->Bind(wxEVT_LEAVE_WINDOW, &MachineObjectPanel::on_mouse_leave, this);
    this->Bind(wxEVT_LEFT_UP, &MachineObjectPanel::on_mouse_left_up, this);
#if 0
#ifdef __APPLE__
    wxPlatformInfo platformInfo;
    auto major = platformInfo.GetOSMajorVersion();
    auto minor = platformInfo.GetOSMinorVersion();
    auto micro = platformInfo.GetOSMicroVersion();

    //macos 13.1.0
    if (major >= 13 && minor >= 1 && micro >= 0) {
        m_is_macos_special_version = true;
    }
#endif
#endif
}


MachineObjectPanel::~MachineObjectPanel() {}

void MachineObjectPanel::set_printer_state(PrinterState state)
{
    m_state = state;
    Refresh();
}

void MachineObjectPanel::show_edit_printer_name(bool show)
{
    m_show_edit = show;
    Refresh();
}

void MachineObjectPanel::show_printer_bind(bool show, PrinterBindState state)
{
    m_show_bind   = show;
    m_bind_state  = state;
    Refresh();
}

void MachineObjectPanel::OnPaint(wxPaintEvent &event)
{
    wxPaintDC dc(this);
    doRender(dc);
}

void MachineObjectPanel::render(wxDC &dc)
{
#ifdef __WXMSW__
    wxSize     size = GetSize();
    wxMemoryDC memdc;
    wxBitmap   bmp(size.x, size.y);
    memdc.SelectObject(bmp);
    memdc.Blit({0, 0}, size, &dc, {0, 0});

    {
        wxGCDC dc2(memdc);
        doRender(dc2);
    }

    memdc.SelectObject(wxNullBitmap);
    dc.DrawBitmap(bmp, 0, 0);
#else
    doRender(dc);
#endif
}

void MachineObjectPanel::doRender(wxDC &dc)
{
    auto   left = 20;
    wxSize size = GetSize();
    dc.SetPen(*wxTRANSPARENT_PEN);

    auto dwbitmap = m_printer_status_offline_lan;
    if (m_state == PrinterState::IDLE) { dwbitmap = m_printer_status_idle; }
    if (m_state == PrinterState::BUSY) { dwbitmap = m_printer_status_busy; }
    if (m_state == PrinterState::OFFLINE_LAN) { dwbitmap = m_printer_status_offline_lan; }
    if (m_state == PrinterState::OFFLINE_WAN) { dwbitmap = m_printer_status_offline_wan; }
    if (m_state == PrinterState::ONLINE_LAN) { dwbitmap = m_printer_online_lan; }
    if (m_state == PrinterState::ONLINE_WAN) { dwbitmap = m_printer_online_wan; }

    // dc.DrawCircle(left, size.y / 2, 3);
    dc.DrawBitmap(dwbitmap.bmp(), wxPoint(left, (size.y - dwbitmap.GetBmpSize().y) / 2));

    left += dwbitmap.GetBmpSize().x + 8;
    dc.SetFont(Label::Body_13);
    dc.SetBackgroundMode(wxTRANSPARENT);
    dc.SetTextForeground(select_machine_light_text(SELECT_MACHINE_GREY900));
    wxString dev_name = "";
    if (m_devInfo) {
        dev_name = from_u8(m_devInfo->get_dev_name());

         /*if (m_state == PrinterState::IN_LAN) {
             dev_name += _L("(LAN)");
         }*/
    }
    auto        sizet        = dc.GetTextExtent(dev_name);
    auto        text_end     = 0;

    if (m_show_edit) {
        text_end = size.x - 30;
    }
    else {
        text_end = size.x;
    }

    wxString finally_name =  dev_name;
    if (sizet.x > (text_end - left)) {
        auto limit_width = text_end - left - dc.GetTextExtent("...").x - 15;
        for (auto i = 0; i < dev_name.length(); i++) {
            auto curr_width = dc.GetTextExtent(dev_name.substr(0, i));
            if (curr_width.x >= limit_width) {
                finally_name = dev_name.substr(0, i) + "...";
                break;
            }
        }
    }

    dc.DrawText(finally_name, wxPoint(left, (size.y - sizet.y) / 2));


    if (m_hover || m_is_macos_special_version) {

        if (m_hover && !m_is_macos_special_version) {
            dc.SetPen(SELECT_MACHINE_BRAND);
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawRectangle(0, 0, size.x, size.y);
        }

        if (m_show_bind) {
            if (m_bind_state == ALLOW_UNBIND) {
                left = size.x - 6;
            }
        }

        if (m_show_edit) {
            left = size.x - 6 - m_edit_name_img.GetBmpSize().x - 6;
        }
    }

}

void MachineObjectPanel::update_machine_info(MachineObject *info, bool is_my_devices)
{
    m_info = info;
    m_is_my_devices = is_my_devices;
    Refresh();
}

void MachineObjectPanel::update_device_info(DeviceObject *info, bool is_my_devices /* = false*/)
{
    m_devInfo = info;
    m_is_my_devices = is_my_devices;
    Refresh();
}

DeviceObject *MachineObjectPanel::device_info() 
{ 
    return m_devInfo;
}

void MachineObjectPanel::on_mouse_enter(wxMouseEvent &evt)
{
    m_hover = true;
    Refresh();
}

void MachineObjectPanel::on_mouse_leave(wxMouseEvent &evt)
{
    m_hover = false;
    Refresh();
}

void MachineObjectPanel::SetHover(bool hover)
{
    if (m_hover != hover) {
        m_hover = hover;
        Refresh();
    }
}

void MachineObjectPanel::SetPressed(bool pressed, bool hit)
{
    //BOOST_LOG_TRIVIAL(info) << "Set pressed: " << pressed << ", " << hit << ", current: " << m_press_flag;
    //flush_logs();
    if (m_press_flag != pressed) {
        m_press_flag = pressed;
        Refresh();
        if (hit) {
            if (pressed) {
                ///mouseDownEvent();
            } else {
                //mouseUpEvent();
            }
        }
    }
}

void MachineObjectPanel::on_mouse_left_up(wxMouseEvent &evt)
{
#ifdef __APPLE__
    if(m_devInfo){
        BOOST_LOG_TRIVIAL(info) << "MachineObjectPanel::on_mouse_left_up--" << m_devInfo->get_dev_name();
    } else {
        BOOST_LOG_TRIVIAL(info) << "MachineObjectPanel::on_mouse_left_up--dev empty";
    }
    
    if(SelectMachinePopup::m_wan_bind_enable){
        BOOST_LOG_TRIVIAL(info) << "MachineObjectPanel::on_mouse_left_up, true";
        return;
    }
#endif
    if (m_is_my_devices) {
        // show edit
        if (m_show_edit) {
            auto edit_left   = GetSize().x - 6 - m_edit_name_img.GetBmpSize().x - 6;
            auto edit_right  = edit_left + m_edit_name_img.GetBmpSize().x;
            auto edit_top    = (GetSize().y - m_edit_name_img.GetBmpSize().y) / 2;
            auto edit_bottom = (GetSize().y - m_edit_name_img.GetBmpSize().y) / 2 + m_edit_name_img.GetBmpSize().y;
            if ((evt.GetPosition().x >= edit_left && evt.GetPosition().x <= edit_right) && evt.GetPosition().y >= edit_top && evt.GetPosition().y <= edit_bottom) {
                wxCommandEvent event(EVT_EDIT_PRINT_NAME);
                event.SetEventObject(this);
                wxPostEvent(this, event);
                return;
            }
        }
        if (m_show_bind) {
            DeviceObject *obj    = nullptr;
            if (is_wan_offline_lan_unbind(obj)) {
                if (obj) {
                    if (obj->is_lan_mode_printer()) {
                        //ConnectPrinterDialog dlg(wxGetApp().mainframe, wxID_ANY, _L("Input access code"));
#ifdef __APPLE__
                        BOOST_LOG_TRIVIAL(info) << "MachineObjectPanel::on_mouse_left_up, wan offline lan connect";
                        SelectMachinePopup::m_wan_bind_enable = true;
#endif
                        ConnectPrinterDialog dlg;
                        dlg.set_device_object(obj);
                        if (dlg.ShowModal() == wxID_OK) {
                            wxGetApp().mainframe->jump_to_monitor(obj->get_dev_id());
                        }
#ifdef __APPLE__
                        SelectMachinePopup::m_wan_bind_enable = false;
#endif
                    }
                }
            } else {
                if (m_devInfo) {
                    wxGetApp().mainframe->jump_to_monitor(m_devInfo->get_dev_id());
                }
                //wxGetApp().mainframe->SetFocus();
                wxCommandEvent event(EVT_DISSMISS_MACHINE_LIST);
                event.SetEventObject(this->GetParent());
                wxPostEvent(this->GetParent(), event);
            }
            return;
        }
        if (m_devInfo && m_devInfo->is_lan_mode_printer()) {
            if (m_devInfo->has_access_right() && m_devInfo->is_avaliable()) {
                wxGetApp().mainframe->jump_to_monitor(m_devInfo->get_dev_id());
            } else {
#ifdef __APPLE__
                BOOST_LOG_TRIVIAL(info) << "MachineObjectPanel::on_mouse_left_up, lan connect";
                SelectMachinePopup::m_wan_bind_enable = true;
#endif
                wxCommandEvent event(EVT_CONNECT_LAN_PRINT);
                event.SetEventObject(this);
                wxPostEvent(this, event);
            }
        } else {
            wxGetApp().mainframe->jump_to_monitor(m_devInfo->get_dev_id());
        }
    } else {
        if (m_devInfo && m_devInfo->is_lan_mode_in_scan_print()) {
#ifdef __APPLE__
            BOOST_LOG_TRIVIAL(info) << "MachineObjectPanel::on_mouse_left_up, lan connect 2";
            SelectMachinePopup::m_wan_bind_enable = true;
#endif
            wxCommandEvent event(EVT_CONNECT_LAN_PRINT);
            event.SetEventObject(this);
            wxPostEvent(this, event);
        } else {
#ifdef __APPLE__
            SelectMachinePopup::m_wan_bind_enable = true;
#endif
            if (!LoginDialog::IsUsrLogin()) {
#ifdef __APPLE__
                BOOST_LOG_TRIVIAL(info) << "MachineObjectPanel::on_mouse_left_up, before IsUsrLogin";
#endif
                MessageDialog msg_wingow(nullptr, _L("Please login first."), "", wxAPPLY | wxOK);
                msg_wingow.ShowModal();
#ifdef __APPLE__
                SelectMachinePopup::m_wan_bind_enable = false;
#endif
            } else {
#ifdef __APPLE__
                BOOST_LOG_TRIVIAL(info) << "MachineObjectPanel::on_mouse_left_up, bind";
#endif
                wxCommandEvent event(EVT_BIND_MACHINE);
                event.SetEventObject(this);
                wxPostEvent(this, event);
            }
            flush_logs();
        }
    }
}

bool MachineObjectPanel::is_wan_offline_lan_unbind(DeviceObject *&obj)
{
    if (m_devInfo->is_lan_mode_printer() || m_devInfo->is_online()) {
        return false;
    }
    DeviceObjectOpr                      *devOpr = wxGetApp().getDeviceObjectOpr();
    std::map<std::string, DeviceObject *> scan_dev_list, local_dev_list;
    devOpr->get_local_machine(local_dev_list);
    auto it = local_dev_list.find(m_devInfo->get_dev_id());
    if (it != local_dev_list.end()) {
        return false;
    }
    devOpr->get_scan_machine(scan_dev_list);
    it = scan_dev_list.find(m_devInfo->get_dev_id());
    if (it == scan_dev_list.end()) {
        return false;
    }
    if (it->second->has_access_right()) {
        return false;
    }
    obj = it->second;
    return true;
}

#ifdef __WINDOWS__
SelectMachinePopup::SelectMachinePopup(wxWindow *parent)
    : PopupWindow(parent, wxBORDER_NONE | wxPU_CONTAINS_CONTROLS), m_dismiss(false), m_updateConnect(false)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__

    /*SetSize(SELECT_MACHINE_POPUP_SIZE);
    SetMinSize(SELECT_MACHINE_POPUP_SIZE);
    SetMaxSize(SELECT_MACHINE_POPUP_SIZE);*/

    Freeze();
    wxBoxSizer *m_sizer_main = new wxBoxSizer(wxVERTICAL);
    //SetBackgroundColour(*wxRED/*SELECT_MACHINE_GREY400*/);



    m_scrolledWindow = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, SELECT_MACHINE_LIST_SIZE, wxHSCROLL | wxVSCROLL);
    m_scrolledWindow->SetBackgroundColour(/**wxWHITE*/ wxColour("#fafafa"));
    m_scrolledWindow->SetMinSize(SELECT_MACHINE_LIST_SIZE);
    m_scrolledWindow->SetScrollRate(0, 30);
    auto m_sizxer_scrolledWindow = new wxBoxSizer(wxVERTICAL);
    m_scrolledWindow->SetSizer(m_sizxer_scrolledWindow);
    m_scrolledWindow->Layout();
    m_sizxer_scrolledWindow->Fit(m_scrolledWindow);

#if !BBL_RELEASE_TO_PUBLIC && defined(__WINDOWS__)
    m_sizer_search_bar = new wxBoxSizer(wxVERTICAL);
    m_search_bar = new wxSearchCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    m_search_bar->ShowSearchButton( true );
    m_search_bar->ShowCancelButton( false );
    m_sizer_search_bar->Add( m_search_bar, 1, wxALL| wxEXPAND, 1 );
    m_sizer_main->Add(m_sizer_search_bar, 0, wxALL | wxEXPAND, FromDIP(2));
    m_search_bar->Bind( wxEVT_COMMAND_TEXT_UPDATED, &SelectMachinePopup::update_machine_list, this );
#endif
    auto own_title        = create_title_panel(_L("My Device"));
    m_sizer_my_devices    = new wxBoxSizer(wxVERTICAL);
    auto seperate_line = new wxPanel(m_scrolledWindow, wxID_ANY, wxDefaultPosition, wxSize(SELECT_MACHINE_ITEM_SIZE.x, FromDIP(1)), wxTAB_TRAVERSAL);
    seperate_line->SetBackgroundColour(SELECT_MACHINE_GREY400);
    auto other_title      = create_title_panel(_L("Other Device"));
    m_sizer_other_devices = new wxBoxSizer(wxVERTICAL);

    m_sizxer_scrolledWindow->Add(own_title, 0, wxEXPAND | wxLEFT, FromDIP(15));
    m_sizxer_scrolledWindow->Add(m_sizer_my_devices, 0, wxEXPAND, 0);
    m_sizxer_scrolledWindow->Add(seperate_line, 0, wxALL | wxALIGN_CENTER, 0);
    m_sizxer_scrolledWindow->Add(other_title, 0, wxEXPAND | wxLEFT, FromDIP(15));
    m_sizxer_scrolledWindow->Add(m_sizer_other_devices, 0, wxEXPAND, 0);


    m_sizer_main->Add(m_scrolledWindow, 0, wxALL | wxEXPAND, FromDIP(0));

    SetSizer(m_sizer_main);
    Layout();
    Thaw();

    #ifdef __APPLE__
        m_scrolledWindow->Bind(wxEVT_LEFT_UP, &SelectMachinePopup::OnLeftUp, this);
    //m_scrolledWindow->Bind(wxEVT_LEFT_DCLICK, &SelectMachinePopup::on_dclick_up, this);
    #endif // __APPLE__

    m_refresh_timer = new wxTimer();
    m_refresh_timer->SetOwner(this);
    Bind(EVT_UPDATE_USER_MACHINE_LIST, &SelectMachinePopup::update_machine_list, this);
    Bind(wxEVT_TIMER, &SelectMachinePopup::on_timer, this);
    Bind(EVT_DISSMISS_MACHINE_LIST, &SelectMachinePopup::on_dissmiss_win, this);

    MultiComMgr::inst()->Bind(COM_CONNECTION_EXIT_EVENT, &SelectMachinePopup::on_connect_exit, this);
    MultiComMgr::inst()->Bind(COM_CONNECTION_READY_EVENT, &SelectMachinePopup::on_connect_ready, this);
    wxGetApp().getDeviceObjectOpr()->Bind(EVT_DEVICE_LIST_UPDATED, &SelectMachinePopup::on_devList_Updated, this);
}

SelectMachinePopup::~SelectMachinePopup()
{
    delete m_refresh_timer;
    m_refresh_timer = nullptr;
    MultiComMgr::inst()->Unbind(COM_CONNECTION_EXIT_EVENT, &SelectMachinePopup::on_connect_exit, this);
    MultiComMgr::inst()->Unbind(COM_CONNECTION_READY_EVENT, &SelectMachinePopup::on_connect_ready, this);
}

void SelectMachinePopup::Popup(wxWindow *WXUNUSED(focus))
{
    BOOST_LOG_TRIVIAL(trace) << "get_print_info: start";
    m_updateConnect = true;
    if (m_refresh_timer) {
        m_refresh_timer->Stop();
        m_refresh_timer->Start(MACHINE_LIST_REFRESH_INTERVAL);
    }

    if (wxGetApp().is_user_login()) {
        if (!get_print_info_thread) {
            get_print_info_thread = new boost::thread(Slic3r::create_thread([&] {
                NetworkAgent* agent = wxGetApp().getAgent();
                unsigned int http_code;
                std::string body;
                int result = agent->get_user_print_info(&http_code, &body);
                if (result == 0) {
                    m_print_info = body;
                } else {
                    m_print_info = "";
                }
                wxCommandEvent event(EVT_UPDATE_USER_MACHINE_LIST);
                event.SetEventObject(this);
                wxPostEvent(this, event);
            }));
        }
    }

    CallAfter([this] {
        wxGetApp().reset_to_active();
        wxCommandEvent user_event(EVT_UPDATE_USER_MACHINE_LIST);
        user_event.SetEventObject(this);
        wxPostEvent(this, user_event);
    });
    PopupWindow::Popup();
}

void SelectMachinePopup::OnDismiss()
{
    BOOST_LOG_TRIVIAL(trace) << "get_print_info: dismiss";
    m_dismiss = true;

    if (m_refresh_timer) {
        m_refresh_timer->Stop();
    }
    if (get_print_info_thread) {
        if (get_print_info_thread->joinable()) {
            get_print_info_thread->join();
            delete get_print_info_thread;
            get_print_info_thread = nullptr;
        }
    }

    wxCommandEvent event(EVT_FINISHED_UPDATE_MACHINE_LIST);
    event.SetEventObject(this);
    wxPostEvent(this, event);
    wxPopupTransientWindow::Dismiss();
}

bool SelectMachinePopup::ProcessLeftDown(wxMouseEvent &event)
{
    return PopupWindow::ProcessLeftDown(event);
}

bool SelectMachinePopup::Show(bool show) {
    if (show) {
        for (int i = 0; i < m_user_list_machine_panel.size(); i++) {
            m_user_list_machine_panel[i]->mPanel->update_device_info(nullptr);
            m_user_list_machine_panel[i]->mPanel->Hide();
        }

        for (int j = 0; j < m_other_list_machine_panel.size(); j++) {
            m_other_list_machine_panel[j]->mPanel->update_device_info(nullptr);
            m_other_list_machine_panel[j]->mPanel->Hide();
        }
    }
    return PopupWindow::Show(show);
}

wxWindow *SelectMachinePopup::create_title_panel(wxString text)
{
    auto panel_title = new wxWindow(m_scrolledWindow, wxID_ANY, wxDefaultPosition, SELECT_MACHINE_ITEM_SIZE, wxTAB_TRAVERSAL);
    panel_title->SetBackgroundColour(/**wxWHITE*/ wxColour("#fafafa"));

    wxBoxSizer *sizer_title = new wxBoxSizer(wxHORIZONTAL);

    auto titleStaticText = new wxStaticText(panel_title, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, 0);
    titleStaticText->Wrap(-1);
    apply_select_machine_title_style(titleStaticText);
    sizer_title->Add(titleStaticText, 0, wxALIGN_CENTER, 0);
    sizer_title->Add(0, 0, 0, wxLEFT, FromDIP(10));

    panel_title->SetSizer(sizer_title);
    panel_title->Layout();
    return panel_title;
}

void SelectMachinePopup::on_connect_exit(ComConnectionExitEvent &event)
{
    event.Skip();
    if (this->IsShown() && m_refresh_timer && !m_refresh_timer->IsRunning()) {
        m_refresh_timer->Start(MACHINE_LIST_REFRESH_INTERVAL);
    }
    //DeviceObjectOpr *devOpr = wxGetApp().getDeviceObjectOpr();
    //id_connect_mode  mode;
    //string dev_id = devOpr->find_dev_from_id(mode, event.id);
    //if (dev_id.empty()) {
    //    return;
    //}
    //std::map<std::string, DeviceObject *> userList;
    //std::map<std::string, DeviceObject *> localList;
    //devOpr->get_user_machine(userList);
    //devOpr->get_local_machine(localList);
    //auto it = localList.find(dev_id);
    //if (it != localList.end() && !it->second->is_online() && it->second->device_type() == DT_BOTH) {
    //    it = userList.find(dev_id);
    //    if (it != userList.end() && it->second->is_online()) {
    //        DeviceObject *userDev = it->second;
    //        for (int j = 0; j < m_user_list_machine_panel.size(); j++) {
    //            DeviceObject *dev = m_user_list_machine_panel[j]->mPanel->device_info();
    //            if (dev->get_dev_id() == dev_id) {
    //                m_user_list_machine_panel[j]->mPanel->update_device_info(userDev, true);
    //                Refresh();
    //                break;
    //            }
    //        }
    //    }
    //}
}

void SelectMachinePopup::on_connect_ready(ComConnectionReadyEvent &event)
{
    if (this->IsShown() && m_refresh_timer && !m_refresh_timer->IsRunning()) {
        m_refresh_timer->Start(MACHINE_LIST_REFRESH_INTERVAL);
    }
    event.Skip();
}

void SelectMachinePopup::on_devList_Updated(DeviceListUpdateEvent &event)
{
    event.Skip();
    if (this->IsShown() && m_refresh_timer && !m_refresh_timer->IsRunning()) {
        m_refresh_timer->Start(MACHINE_LIST_REFRESH_INTERVAL);
    }
}


void SelectMachinePopup::on_timer(wxTimerEvent &event)
{
    BOOST_LOG_TRIVIAL(trace) << "SelectMachinePopup on_timer";
    wxGetApp().reset_to_active();
    wxCommandEvent user_event(EVT_UPDATE_USER_MACHINE_LIST);
    user_event.SetEventObject(this);
    wxPostEvent(this, user_event);
}

void SelectMachinePopup::update_other_devices()
{
    DeviceObjectOpr *devOpr = wxGetApp().getDeviceObjectOpr();
    m_free_device_list.clear();
    devOpr->get_scan_machine(m_free_device_list);

    // sort list
    std::vector<std::pair<std::string, DeviceObject *>> other_machine_list;
    for (auto &it : m_free_device_list) {
        other_machine_list.push_back(it);
    }

    std::sort(other_machine_list.begin(), other_machine_list.end(), [](const auto &a, const auto &b) {
        if (a.second == b.second)
            return false;
        if (!a.second)
            return false;
        if (!b.second)
            return true;
        if (a.second->connectMode() != b.second->connectMode())
            return a.second->connectMode() < b.second->connectMode();
        return a.second->get_dev_name().compare(b.second->get_dev_name()) < 0;
    });

    BOOST_LOG_TRIVIAL(trace) << "SelectMachinePopup update_other_devices start";
    this->Freeze();
    m_scrolledWindow->Freeze();
    int i = 0;

    for (auto &elem : other_machine_list) {
        DeviceObject* deviceObj = elem.second;
        //MachineObject *     mobj = elem.second;
        /* do not show printer bind state is empty */
        //if (!mobj->is_avaliable()) continue;

        /*if (!wxGetApp().is_user_login())
            continue; */

        /* do not show printer in my list */
        auto it = m_bind_machine_list.find(deviceObj->get_dev_id());
        if (it != m_bind_machine_list.end())
            continue;

        MachineObjectPanel* op = nullptr;
        if (i < m_other_list_machine_panel.size()) {
            op = m_other_list_machine_panel[i]->mPanel;
            op->Show();
        } else {
            op = new MachineObjectPanel(m_scrolledWindow, wxID_ANY);
            MachinePanel* mpanel = new MachinePanel();
            mpanel->mIndex = wxString::Format("%d", i);
            mpanel->mPanel = op;
            m_other_list_machine_panel.push_back(mpanel);
            m_sizer_other_devices->Add(op, 0, wxEXPAND, 0);
        }
        i++;

        op->update_device_info(deviceObj);

        if (deviceObj->is_lan_mode_in_scan_print()) {
            op->set_printer_state(PrinterState::OFFLINE_LAN);
            /*if (deviceObj->has_access_right()) {
                op->set_printer_state(PrinterState::IN_LAN);
            } else {
                op->set_printer_state(PrinterState::LOCK);
            }*/
        } else {
            op->set_printer_state(PrinterState::OFFLINE_WAN);
            /*op->show_edit_printer_name(false);
            op->show_printer_bind(true, PrinterBindState::ALLOW_BIND);
            if (deviceObj->is_in_printing()) {
                op->set_printer_state(PrinterState::BUSY);
            } else {
                op->SetToolTip(_L("Online"));
                op->set_printer_state(IDLE);
            }*/
        }

        op->Bind(EVT_CONNECT_LAN_PRINT, [this, deviceObj](wxCommandEvent &e) {
            if (deviceObj) {
                if (deviceObj->is_lan_mode_printer()) {
                    //ConnectPrinterDialog dlg(wxGetApp().mainframe, wxID_ANY, _L("Input access code"));
                    ConnectPrinterDialog dlg;
                    dlg.set_device_object(deviceObj);
                    if (dlg.ShowModal() == wxID_OK) {
                        wxGetApp().mainframe->jump_to_monitor(deviceObj->get_dev_id());
                    }
                }
            }
#ifdef __APPLE__
            m_wan_bind_enable = false;
#endif
        });

        op->Bind(EVT_BIND_MACHINE, [this, deviceObj](wxCommandEvent &e) {
            if (!deviceObj)
                return;
            BindInfo*          info = deviceObj->get_bind_info();
            BindMachineDialog dlg;
            dlg.update_device_info2(info);
            int dlg_result = wxID_CANCEL;
            dlg_result     = dlg.ShowModal();
            if (dlg_result == wxID_OK) { wxGetApp().mainframe->jump_to_monitor(deviceObj->get_dev_id()); }
#ifdef __APPLE__
            m_wan_bind_enable = false;
#endif
        });
    }

    for (int j = i; j < m_other_list_machine_panel.size(); j++) {
        m_other_list_machine_panel[j]->mPanel->update_device_info(nullptr);
        m_other_list_machine_panel[j]->mPanel->Hide();
    }

    if(m_other_devices_count != i) {
        m_scrolledWindow->Fit();
    }
    m_scrolledWindow->Layout();
    m_scrolledWindow->Thaw();
    Layout();
    Fit();
    this->Thaw();
    m_other_devices_count = i;
    BOOST_LOG_TRIVIAL(trace) << "SelectMachinePopup update_other_devices end";
}

void SelectMachinePopup::update_user_devices()
{
    DeviceObjectOpr *devOpr = wxGetApp().getDeviceObjectOpr();
    if (!devOpr)
        return;

    m_bind_machine_list.clear();
    devOpr->get_my_machine_list(m_bind_machine_list);

    //sort list
    std::vector<std::pair<std::string, DeviceObject *>> user_machine_list;
    for (auto& it: m_bind_machine_list) {
        user_machine_list.push_back(it);
    }

    std::sort(user_machine_list.begin(), user_machine_list.end(), [](const auto &a, const auto &b) {
        if (a.second == b.second)
            return false;
        if (!a.second)
            return false;
        if (!b.second)
            return true;
        if (a.second->is_online() != b.second->is_online())
            return a.second->is_online();
        if (a.second->is_lan_mode_printer() != b.second->is_lan_mode_printer())
            return a.second->is_lan_mode_printer();
        return a.second->get_dev_name().compare(b.second->get_dev_name()) < 0;
    });

    BOOST_LOG_TRIVIAL(trace) << "SelectMachinePopup update_machine_list start";
    this->Freeze();
    m_scrolledWindow->Freeze();
    int i = 0;

    for (auto& elem : user_machine_list) {
        DeviceObject       *devObj = elem.second;
        MachineObjectPanel* op = nullptr;
        if (i < m_user_list_machine_panel.size()) {
            op = m_user_list_machine_panel[i]->mPanel;
            op->Show();
        } else {
            op = new MachineObjectPanel(m_scrolledWindow, wxID_ANY);
            MachinePanel* mpanel = new MachinePanel();
            mpanel->mIndex = wxString::Format("%d", i);
            mpanel->mPanel = op;
            m_user_list_machine_panel.push_back(mpanel);
            m_sizer_my_devices->Add(op, 0, wxEXPAND, 0);
        }
        i++;
        op->update_device_info(devObj, true);
        //set in lan
        if (devObj->is_lan_mode_printer()) {
            if (!devObj->is_online()) {
                op->SetToolTip(_L(""));
                op->show_printer_bind(true, PrinterBindState::ALLOW_UNBIND);
                if (devObj->is_lan_mode_in_scan_print()) {
                    op->set_printer_state(PrinterState::OFFLINE_LAN);
                    if (m_updateConnect && devObj->get_lan_dev_info() != nullptr) {
                        /*m_updateConnect = false;
                        m_refresh_timer->Stop();*/
                        devOpr->set_selected_machine(devObj->get_dev_id(), true);
                    }
                } else {
                    op->set_printer_state(PrinterState::OFFLINE_WAN);
                }
            }
            else {
                op->show_printer_bind(false, PrinterBindState::NONE);
                //op->show_edit_printer_name(false);
                if (devObj->has_access_right() && devObj->is_avaliable()) {
                    op->set_printer_state(PrinterState::ONLINE_LAN);
                    op->show_printer_bind(true, PrinterBindState::ALLOW_UNBIND);
                    op->SetToolTip(_L("Online"));
                }
                else {
                    op->set_printer_state(PrinterState::OFFLINE_LAN);
                }
            }
            op->Bind(EVT_UNBIND_MACHINE, [this, devOpr, devObj](wxCommandEvent &e) {
                MessageDialog msg_wingow(nullptr, _L("Are you sure to unbind this device?"), _L("Question"), wxYES_NO);
                if (wxID_YES == msg_wingow.ShowModal()) {
#ifdef __APPLE__
                    m_wan_bind_enable = false;
#endif
                    devOpr->unbind_lan_machine(devObj);

                    MessageDialog msg_wingow1(nullptr, _L("Log out successful."), "", wxAPPLY | wxOK);
                    if (msg_wingow1.ShowModal() == wxOK) {
                        return;
                    }
                }
#ifdef __APPLE__
                m_wan_bind_enable = false;
#endif
            });
        }
        else {
            op->show_printer_bind(true, PrinterBindState::ALLOW_UNBIND);
            op->Bind(EVT_UNBIND_MACHINE, [this, devObj, devOpr](wxCommandEvent& e) {
                // show_unbind_dialog
                if (!devObj){
#ifdef __APPLE__
                    m_wan_bind_enable = false;
#endif
                    return;
                }
                BindInfo*            info = devObj->get_bind_info();
                UnBindMachineDialog dlg;
                dlg.update_device_info2(info);
                dlg.ShowModal();
                /*if (dlg.ShowModal() == wxID_OK) {
                    devOpr->set_selected_machine("");
                }*/
#ifdef __APPLE__
                m_wan_bind_enable = false;
#endif
                });
            string name = devObj->get_dev_name();
            if (!devObj->is_online()) {
                op->SetToolTip(_L("Offline"));
                op->set_printer_state(PrinterState::OFFLINE_WAN);
            }
            else {
                //op->show_edit_printer_name(true);
                op->show_printer_bind(true, PrinterBindState::ALLOW_UNBIND);
                if (devObj->is_in_printing()) {
                    op->SetToolTip(_L("Busy"));
                    op->set_printer_state(PrinterState::BUSY);
                }
                else {
                    op->SetToolTip(_L("Online"));
                    op->set_printer_state(PrinterState::ONLINE_WAN);
                }
            }
        }

        op->Bind(EVT_CONNECT_LAN_PRINT, [this, devObj](wxCommandEvent &e) {
            if (devObj) {
                if (devObj->is_lan_mode_printer()) {
                    //ConnectPrinterDialog dlg(wxGetApp().mainframe, wxID_ANY, _L("Input access code"));
                    ConnectPrinterDialog dlg;
                    dlg.set_device_object(devObj);
                    if (dlg.ShowModal() == wxID_OK) {
                        wxGetApp().mainframe->jump_to_monitor(devObj->get_dev_id());
                    }
                }
            }
#ifdef __APPLE__
            m_wan_bind_enable = false;
#endif
        });

         /*op->Bind(EVT_EDIT_PRINT_NAME, [this, devObj](wxCommandEvent &e) {
            EditDevNameDialog dlg;
            dlg.set_machine_obj(mobj);
            dlg.ShowModal();
         });*/
    }

    if (m_updateConnect) {
        m_updateConnect = false;
        m_refresh_timer->Stop();
    }

    for (int j = i; j < m_user_list_machine_panel.size(); j++) {
        m_user_list_machine_panel[j]->mPanel->update_device_info(nullptr);
        m_user_list_machine_panel[j]->mPanel->Hide();
    }
    //m_sizer_my_devices->Layout();

    if (m_my_devices_count != i) {
        m_scrolledWindow->Fit();
    }
    m_scrolledWindow->Layout();
    m_scrolledWindow->Thaw();
    Layout();
    Fit();
    this->Thaw();
    m_my_devices_count = i;
}

bool SelectMachinePopup::search_for_printer(MachineObject* obj)
{
    std::string search_text = std::string((m_search_bar->GetValue()).mb_str());
    if (search_text.empty()) {
        return true;
    }
    auto name = obj->get_dev_name();
    auto ip = obj->get_dev_ip();
    auto name_it = name.find(search_text);
    auto ip_it = ip.find(search_text);
    if ((name_it != std::string::npos)||(ip_it != std::string::npos)) {
        return true;
    }

    return false;
}

void SelectMachinePopup::on_dissmiss_win(wxCommandEvent &event)
{
    Dismiss();
}

void SelectMachinePopup::update_machine_list(wxCommandEvent &event)
{
    DeviceObjectOpr *devOpr = wxGetApp().getDeviceObjectOpr();
    devOpr->update_scan_machine();

    update_user_devices();
    update_other_devices();
    BOOST_LOG_TRIVIAL(trace) << "SelectMachinePopup update_machine_list end";
}

void SelectMachinePopup::on_dclick_up(wxMouseEvent &event)
{
    //BOOST_LOG_TRIVIAL(info) << "SelectMachinePopup on_dclick_up";
    //flush_logs();
    //event.Skip();
}

void SelectMachinePopup::OnLeftUp(wxMouseEvent &event)
{
    auto mouse_pos = ClientToScreen(event.GetPosition());
    auto wxscroll_win_pos = m_scrolledWindow->ClientToScreen(wxPoint(0, 0));
#ifdef __APPLE__
    BOOST_LOG_TRIVIAL(info) << "SelectMachinePopup uOnLeftUp";
#endif
    if (mouse_pos.x > wxscroll_win_pos.x && mouse_pos.y > wxscroll_win_pos.y && mouse_pos.x < (wxscroll_win_pos.x + m_scrolledWindow->GetSize().x) &&
        mouse_pos.y < (wxscroll_win_pos.y + m_scrolledWindow->GetSize().y)) {
        for (MachinePanel* p : m_user_list_machine_panel) {
            auto p_rect = p->mPanel->ClientToScreen(wxPoint(0, 0));
            if (mouse_pos.x > p_rect.x && mouse_pos.y > p_rect.y && mouse_pos.x < (p_rect.x + p->mPanel->GetSize().x) && mouse_pos.y < (p_rect.y + p->mPanel->GetSize().y)) {
#ifdef __APPLE__
                if(!p->mPanel->device_info())
                    break;
#endif
                wxMouseEvent event(wxEVT_LEFT_UP);
                auto         tag_pos = p->mPanel->ScreenToClient(mouse_pos);
                event.SetPosition(tag_pos);
                event.SetEventObject(p->mPanel);
                wxPostEvent(p->mPanel, event);
#ifdef __APPLE__
                break;
#endif
            }
        }

        for (MachinePanel* p : m_other_list_machine_panel) {
            auto p_rect = p->mPanel->ClientToScreen(wxPoint(0, 0));
            if (mouse_pos.x > p_rect.x && mouse_pos.y > p_rect.y && mouse_pos.x < (p_rect.x + p->mPanel->GetSize().x) && mouse_pos.y < (p_rect.y + p->mPanel->GetSize().y)) {
#ifdef __APPLE__
                if(!p->mPanel->device_info())
                    break;
                BOOST_LOG_TRIVIAL(info) << "SelectMachinePopup uOnLeftUp---" << p->mPanel->device_info()->get_dev_name();
#endif
                wxMouseEvent event(wxEVT_LEFT_UP);
                auto         tag_pos = p->mPanel->ScreenToClient(mouse_pos);
                event.SetPosition(tag_pos);
                event.SetEventObject(p->mPanel);
                wxPostEvent(p->mPanel, event);
#ifdef __APPLE__
                break;
#endif
            }
        }
        flush_logs();
        //hyper link
        /*auto h_rect = m_hyperlink->ClientToScreen(wxPoint(0, 0));
        if (mouse_pos.x > h_rect.x && mouse_pos.y > h_rect.y && mouse_pos.x < (h_rect.x + m_hyperlink->GetSize().x) && mouse_pos.y < (h_rect.y + m_hyperlink->GetSize().y)) {
          wxLaunchDefaultBrowser(wxT("https://wiki.bambulab.com/en/software/bambu-studio/failed-to-connect-printer"));
        }*/
    }
}
#else // non-Windows (macOS / Linux): "#else if __APPLE__" was a no-op typo for "#else"
SelectMachinePopup::SelectMachinePopup(wxWindow *parent)
    //: PopupWindow(parent, wxBORDER_NONE | wxPU_CONTAINS_CONTROLS), m_dismiss(false), m_updateConnect(false)
:FFPopupWindow(parent), m_dismiss(false), m_updateConnect(false)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__

    /*SetSize(SELECT_MACHINE_POPUP_SIZE);
    SetMinSize(SELECT_MACHINE_POPUP_SIZE);
    SetMaxSize(SELECT_MACHINE_POPUP_SIZE);*/

    Freeze();
    wxBoxSizer *m_sizer_main = new wxBoxSizer(wxVERTICAL);
    //SetBackgroundColour(*wxRED/*SELECT_MACHINE_GREY400*/);



    m_scrolledWindow = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, SELECT_MACHINE_LIST_SIZE, wxHSCROLL | wxVSCROLL);
    m_scrolledWindow->SetBackgroundColour(/**wxWHITE*/ wxColour("#fafafa"));
    m_scrolledWindow->SetMinSize(SELECT_MACHINE_LIST_SIZE);
    m_scrolledWindow->SetScrollRate(0, 30);
    auto m_sizxer_scrolledWindow = new wxBoxSizer(wxVERTICAL);
    m_scrolledWindow->SetSizer(m_sizxer_scrolledWindow);
    m_scrolledWindow->Layout();
    m_sizxer_scrolledWindow->Fit(m_scrolledWindow);

#if !BBL_RELEASE_TO_PUBLIC && defined(__WINDOWS__)
	m_sizer_search_bar = new wxBoxSizer(wxVERTICAL);
	m_search_bar = new wxSearchCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_search_bar->ShowSearchButton( true );
	m_search_bar->ShowCancelButton( false );
	m_sizer_search_bar->Add( m_search_bar, 1, wxALL| wxEXPAND, 1 );
	m_sizer_main->Add(m_sizer_search_bar, 0, wxALL | wxEXPAND, FromDIP(2));
	m_search_bar->Bind( wxEVT_COMMAND_TEXT_UPDATED, &SelectMachinePopup::update_machine_list, this );
#endif
    auto own_title        = create_title_panel(_L("My Device"));
    m_sizer_my_devices    = new wxBoxSizer(wxVERTICAL);
    auto seperate_line = new wxPanel(m_scrolledWindow, wxID_ANY, wxDefaultPosition, wxSize(SELECT_MACHINE_ITEM_SIZE.x, FromDIP(1)), wxTAB_TRAVERSAL);
    seperate_line->SetBackgroundColour(SELECT_MACHINE_GREY400);
    auto other_title      = create_title_panel(_L("Other Device"));
    m_sizer_other_devices = new wxBoxSizer(wxVERTICAL);

    m_sizxer_scrolledWindow->Add(own_title, 0, wxEXPAND | wxLEFT, FromDIP(15));
    m_sizxer_scrolledWindow->Add(m_sizer_my_devices, 0, wxEXPAND, 0);
    m_sizxer_scrolledWindow->Add(seperate_line, 0, wxALL | wxALIGN_CENTER, 0);
    m_sizxer_scrolledWindow->Add(other_title, 0, wxEXPAND | wxLEFT, FromDIP(15));
    m_sizxer_scrolledWindow->Add(m_sizer_other_devices, 0, wxEXPAND, 0);


    m_sizer_main->Add(m_scrolledWindow, 0, wxALL | wxEXPAND, FromDIP(0));

    SetSizer(m_sizer_main);
    Layout();
    Thaw();

    #ifdef __APPLE__
        m_scrolledWindow->Bind(wxEVT_LEFT_UP, &SelectMachinePopup::OnLeftUp, this);
    //m_scrolledWindow->Bind(wxEVT_LEFT_DCLICK, &SelectMachinePopup::on_dclick_up, this);
    #endif // __APPLE__

    m_refresh_timer = new wxTimer();
    m_refresh_timer->SetOwner(this);
    Bind(EVT_UPDATE_USER_MACHINE_LIST, &SelectMachinePopup::update_machine_list, this);
    Bind(wxEVT_TIMER, &SelectMachinePopup::on_timer, this);
    Bind(EVT_DISSMISS_MACHINE_LIST, &SelectMachinePopup::on_dissmiss_win, this);

    MultiComMgr::inst()->Bind(COM_CONNECTION_EXIT_EVENT, &SelectMachinePopup::on_connect_exit, this);
    MultiComMgr::inst()->Bind(COM_CONNECTION_READY_EVENT, &SelectMachinePopup::on_connect_ready, this);
    wxGetApp().getDeviceObjectOpr()->Bind(EVT_DEVICE_LIST_UPDATED, &SelectMachinePopup::on_devList_Updated, this);
}

SelectMachinePopup::~SelectMachinePopup()
{
    delete m_refresh_timer;
    m_refresh_timer = nullptr;
    MultiComMgr::inst()->Unbind(COM_CONNECTION_EXIT_EVENT, &SelectMachinePopup::on_connect_exit, this);
    MultiComMgr::inst()->Unbind(COM_CONNECTION_READY_EVENT, &SelectMachinePopup::on_connect_ready, this);
}

void SelectMachinePopup::Popup(wxWindow *WXUNUSED(focus))
{
    BOOST_LOG_TRIVIAL(trace) << "get_print_info: start";
    m_updateConnect = true;
    if (m_refresh_timer) {
        m_refresh_timer->Stop();
        m_refresh_timer->Start(MACHINE_LIST_REFRESH_INTERVAL);
    }

    if (wxGetApp().is_user_login()) {
        if (!get_print_info_thread) {
            get_print_info_thread = new boost::thread(Slic3r::create_thread([&] {
                NetworkAgent* agent = wxGetApp().getAgent();
                unsigned int http_code;
                std::string body;
                int result = agent->get_user_print_info(&http_code, &body);
                if (result == 0) {
                    m_print_info = body;
                } else {
                    m_print_info = "";
                }
                wxCommandEvent event(EVT_UPDATE_USER_MACHINE_LIST);
                event.SetEventObject(this);
                wxPostEvent(this, event);
            }));
        }
    }

    CallAfter([this] {
        wxGetApp().reset_to_active();
        wxCommandEvent user_event(EVT_UPDATE_USER_MACHINE_LIST);
        user_event.SetEventObject(this);
        wxPostEvent(this, user_event);
    });
    ShowDevList(true);
    FFPopupWindow::Popup();
}

void SelectMachinePopup::OnDismiss()
{
    BOOST_LOG_TRIVIAL(trace) << "get_print_info: dismiss";
    m_dismiss = true;

    if (m_refresh_timer) {
        m_refresh_timer->Stop();
    }
    if (get_print_info_thread) {
        if (get_print_info_thread->joinable()) {
            get_print_info_thread->join();
            delete get_print_info_thread;
            get_print_info_thread = nullptr;
        }
    }

    wxCommandEvent event(EVT_FINISHED_UPDATE_MACHINE_LIST);
    event.SetEventObject(this);
    wxPostEvent(this, event);
}

/*bool SelectMachinePopup::ProcessLeftDown(wxMouseEvent &event)
{
    return PopupWindow::ProcessLeftDown(event);
}*/

bool SelectMachinePopup::ShowDevList(bool show) {
    if (show) {
        for (int i = 0; i < m_user_list_machine_panel.size(); i++) {
            m_user_list_machine_panel[i]->mPanel->update_device_info(nullptr);
            m_user_list_machine_panel[i]->mPanel->Hide();
        }

        for (int j = 0; j < m_other_list_machine_panel.size(); j++) {
            m_other_list_machine_panel[j]->mPanel->update_device_info(nullptr);
            m_other_list_machine_panel[j]->mPanel->Hide();
        }
    }
    return show;
}

wxWindow *SelectMachinePopup::create_title_panel(wxString text)
{
    auto panel_title = new wxWindow(m_scrolledWindow, wxID_ANY, wxDefaultPosition, SELECT_MACHINE_ITEM_SIZE, wxTAB_TRAVERSAL);
    panel_title->SetBackgroundColour(/**wxWHITE*/ wxColour("#fafafa"));

    wxBoxSizer *sizer_title = new wxBoxSizer(wxHORIZONTAL);

    auto titleStaticText = new wxStaticText(panel_title, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, 0);
    titleStaticText->Wrap(-1);
    apply_select_machine_title_style(titleStaticText);
    sizer_title->Add(titleStaticText, 0, wxALIGN_CENTER, 0);
    sizer_title->Add(0, 0, 0, wxLEFT, FromDIP(10));

    panel_title->SetSizer(sizer_title);
    panel_title->Layout();
    return panel_title;
}

void SelectMachinePopup::on_connect_exit(ComConnectionExitEvent &event)
{
    event.Skip();
    if (this->IsShown() && m_refresh_timer && !m_refresh_timer->IsRunning()) {
        BOOST_LOG_TRIVIAL(info) << "on_connect_exit--timer start";
        m_refresh_timer->Start(MACHINE_LIST_REFRESH_INTERVAL);
    }
    //DeviceObjectOpr *devOpr = wxGetApp().getDeviceObjectOpr();
    //id_connect_mode  mode;
    //string dev_id = devOpr->find_dev_from_id(mode, event.id);
    //if (dev_id.empty()) {
    //    return;
    //}
    //std::map<std::string, DeviceObject *> userList;
    //std::map<std::string, DeviceObject *> localList;
    //devOpr->get_user_machine(userList);
    //devOpr->get_local_machine(localList);
    //auto it = localList.find(dev_id);
    //if (it != localList.end() && !it->second->is_online() && it->second->device_type() == DT_BOTH) {
    //    it = userList.find(dev_id);
    //    if (it != userList.end() && it->second->is_online()) {
    //        DeviceObject *userDev = it->second;
    //        for (int j = 0; j < m_user_list_machine_panel.size(); j++) {
    //            DeviceObject *dev = m_user_list_machine_panel[j]->mPanel->device_info();
    //            if (dev->get_dev_id() == dev_id) {
    //                m_user_list_machine_panel[j]->mPanel->update_device_info(userDev, true);
    //                Refresh();
    //                break;
    //            }
    //        }
    //    }        
    //}
}

void SelectMachinePopup::on_connect_ready(ComConnectionReadyEvent &event)
{
    if (this->IsShown() && m_refresh_timer && !m_refresh_timer->IsRunning()) {
        BOOST_LOG_TRIVIAL(info) << "on_connect_ready--timer start";
        m_refresh_timer->Start(MACHINE_LIST_REFRESH_INTERVAL);
    }
    event.Skip();
}

void SelectMachinePopup::on_devList_Updated(DeviceListUpdateEvent &event) 
{
    event.Skip();
    if (this->IsShown() && m_refresh_timer && !m_refresh_timer->IsRunning()) {
        BOOST_LOG_TRIVIAL(info) << "on_devList_Updated--timer start";
        m_refresh_timer->Start(MACHINE_LIST_REFRESH_INTERVAL);
    }
}


void SelectMachinePopup::on_timer(wxTimerEvent &event)
{
    BOOST_LOG_TRIVIAL(trace) << "SelectMachinePopup on_timer";
    wxGetApp().reset_to_active();
    wxCommandEvent user_event(EVT_UPDATE_USER_MACHINE_LIST);
    user_event.SetEventObject(this);
    wxPostEvent(this, user_event);
}

void SelectMachinePopup::update_other_devices()
{
    DeviceObjectOpr *devOpr = wxGetApp().getDeviceObjectOpr();
    m_free_device_list.clear();
    devOpr->get_scan_machine(m_free_device_list);

    // sort list
    std::vector<std::pair<std::string, DeviceObject *>> other_machine_list;
    for (auto &it : m_free_device_list) {
        other_machine_list.push_back(it);
    }

    std::sort(other_machine_list.begin(), other_machine_list.end(), [](const auto &a, const auto &b) {
        if (a.second == b.second)
            return false;
        if (!a.second)
            return false;
        if (!b.second)
            return true;
        if (a.second->connectMode() != b.second->connectMode())
            return a.second->connectMode() < b.second->connectMode();
        return a.second->get_dev_name().compare(b.second->get_dev_name()) < 0;
    });

    BOOST_LOG_TRIVIAL(trace) << "SelectMachinePopup update_other_devices start";
    this->Freeze();
    m_scrolledWindow->Freeze();
    int i = 0;

    for (auto &elem : other_machine_list) {
        DeviceObject* deviceObj = elem.second;
        //MachineObject *     mobj = elem.second;
        /* do not show printer bind state is empty */
        //if (!mobj->is_avaliable()) continue;

        /*if (!wxGetApp().is_user_login())
            continue; */

        /* do not show printer in my list */
        auto it = m_bind_machine_list.find(deviceObj->get_dev_id());
        if (it != m_bind_machine_list.end())
            continue;

        MachineObjectPanel* op = nullptr;
        if (i < m_other_list_machine_panel.size()) {
            op = m_other_list_machine_panel[i]->mPanel;
            op->Show();
#if !BBL_RELEASE_TO_PUBLIC && defined(__WINDOWS__)
			if (!search_for_printer(mobj)) {
				op->Hide();
			}
#endif
        } else {
            op = new MachineObjectPanel(m_scrolledWindow, wxID_ANY);
            MachinePanel* mpanel = new MachinePanel();
            mpanel->mIndex = wxString::Format("%d", i);
            mpanel->mPanel = op;
            m_other_list_machine_panel.push_back(mpanel);
            m_sizer_other_devices->Add(op, 0, wxEXPAND, 0);
        }
        i++;

        op->update_device_info(deviceObj);

        if (deviceObj->is_lan_mode_in_scan_print()) {
            op->set_printer_state(PrinterState::OFFLINE_LAN);
            /*if (deviceObj->has_access_right()) {
                op->set_printer_state(PrinterState::IN_LAN);
            } else {
                op->set_printer_state(PrinterState::LOCK);
            }*/
        } else {
            op->set_printer_state(PrinterState::OFFLINE_WAN);
            /*op->show_edit_printer_name(false);
            op->show_printer_bind(true, PrinterBindState::ALLOW_BIND);
            if (deviceObj->is_in_printing()) {
                op->set_printer_state(PrinterState::BUSY);
            } else {
                op->SetToolTip(_L("Online"));
                op->set_printer_state(IDLE);
            }*/
        }

        op->Bind(EVT_CONNECT_LAN_PRINT, [this, deviceObj](wxCommandEvent &e) {
            if (deviceObj) {
                if (deviceObj->is_lan_mode_printer()) {
                    //ConnectPrinterDialog dlg(wxGetApp().mainframe, wxID_ANY, _L("Input access code"));
                    ConnectPrinterDialog dlg;
                    dlg.set_device_object(deviceObj);
                    if (dlg.ShowModal() == wxID_OK) {
                        wxGetApp().mainframe->jump_to_monitor(deviceObj->get_dev_id());
                    }
                }
            }
#ifdef __APPLE__
            m_wan_bind_enable = false;
#endif
        });

        op->Bind(EVT_BIND_MACHINE, [this, deviceObj](wxCommandEvent &e) {
            if (!deviceObj)
                return;
            BindInfo*          info = deviceObj->get_bind_info();
            BindMachineDialog dlg;
            dlg.update_device_info2(info);
            int dlg_result = wxID_CANCEL;
            dlg_result     = dlg.ShowModal();
            if (dlg_result == wxID_OK) { wxGetApp().mainframe->jump_to_monitor(deviceObj->get_dev_id()); }
#ifdef __APPLE__
            m_wan_bind_enable = false;
#endif
        });
    }

    for (int j = i; j < m_other_list_machine_panel.size(); j++) {
        m_other_list_machine_panel[j]->mPanel->update_device_info(nullptr);
        m_other_list_machine_panel[j]->mPanel->Hide();
    }

    if(m_other_devices_count != i) {
		m_scrolledWindow->Fit();
    }
    m_scrolledWindow->Layout();
	m_scrolledWindow->Thaw();
	Layout();
	Fit();
	this->Thaw();
    m_other_devices_count = i;
    BOOST_LOG_TRIVIAL(trace) << "SelectMachinePopup update_other_devices end";
}

void SelectMachinePopup::update_user_devices()
{
    DeviceObjectOpr *devOpr = wxGetApp().getDeviceObjectOpr();
    if (!devOpr)
        return;

    m_bind_machine_list.clear();
    devOpr->get_my_machine_list(m_bind_machine_list);

    //sort list
    std::vector<std::pair<std::string, DeviceObject *>> user_machine_list;
    for (auto& it: m_bind_machine_list) {
        user_machine_list.push_back(it);
    }

    std::sort(user_machine_list.begin(), user_machine_list.end(), [](const auto &a, const auto &b) {
            if (a.second == b.second)
                return false;
            if (!a.second)
                return false;
            if (!b.second)
                return true;
            if (a.second->is_online() != b.second->is_online())
                return a.second->is_online();
            if (a.second->is_lan_mode_printer() != b.second->is_lan_mode_printer())
                return a.second->is_lan_mode_printer();
            return a.second->get_dev_name().compare(b.second->get_dev_name()) < 0;
        });

    BOOST_LOG_TRIVIAL(trace) << "SelectMachinePopup update_machine_list start";
    this->Freeze();
    m_scrolledWindow->Freeze();
    int i = 0;

    for (auto& elem : user_machine_list) {
        DeviceObject       *devObj = elem.second;
        MachineObjectPanel* op = nullptr;
        if (i < m_user_list_machine_panel.size()) {
            op = m_user_list_machine_panel[i]->mPanel;
            op->Show();
#if !BBL_RELEASE_TO_PUBLIC && defined(__WINDOWS__)
			if (!search_for_printer(mobj)) {
				op->Hide();
			}
#endif
        } else {
            op = new MachineObjectPanel(m_scrolledWindow, wxID_ANY);
            MachinePanel* mpanel = new MachinePanel();
            mpanel->mIndex = wxString::Format("%d", i);
            mpanel->mPanel = op;
            m_user_list_machine_panel.push_back(mpanel);
            m_sizer_my_devices->Add(op, 0, wxEXPAND, 0);
        }
        i++;
        op->update_device_info(devObj, true);
        //set in lan
        if (devObj->is_lan_mode_printer()) {
            if (!devObj->is_online()) {
                op->SetToolTip(_L(""));
                op->show_printer_bind(true, PrinterBindState::ALLOW_UNBIND);
                if (devObj->is_lan_mode_in_scan_print()) {
                    op->set_printer_state(PrinterState::OFFLINE_LAN);
                    if (m_updateConnect && devObj->get_lan_dev_info() != nullptr) {
                        /*m_updateConnect = false;
                        m_refresh_timer->Stop();*/
                        devOpr->set_selected_machine(devObj->get_dev_id(), true);
                    }
                } else {
                    op->set_printer_state(PrinterState::OFFLINE_WAN);
                }
            }
            else {
                op->show_printer_bind(false, PrinterBindState::NONE);
                //op->show_edit_printer_name(false);
                if (devObj->has_access_right() && devObj->is_avaliable()) {
                    op->set_printer_state(PrinterState::ONLINE_LAN);
                    op->show_printer_bind(true, PrinterBindState::ALLOW_UNBIND);
                    op->SetToolTip(_L("Online"));
                }
                else {
                    op->set_printer_state(PrinterState::OFFLINE_LAN);
                }
            }
            op->Bind(EVT_UNBIND_MACHINE, [this, devOpr, devObj](wxCommandEvent &e) {
                MessageDialog msg_wingow(nullptr, _L("Are you sure to unbind this device?"), _L("Question"), wxYES_NO);
                if (wxID_YES == msg_wingow.ShowModal()) {
#ifdef __APPLE__
                    m_wan_bind_enable = false;
#endif
                    devOpr->unbind_lan_machine(devObj);

                    MessageDialog msg_wingow1(nullptr, _L("Log out successful."), "", wxAPPLY | wxOK);
                    if (msg_wingow1.ShowModal() == wxOK) {
                        return;
                    }
                }
#ifdef __APPLE__
                m_wan_bind_enable = false;
#endif
            });
        }
        else {
            op->show_printer_bind(true, PrinterBindState::ALLOW_UNBIND);
            op->Bind(EVT_UNBIND_MACHINE, [this, devObj, devOpr](wxCommandEvent& e) {
                // show_unbind_dialog
                if (!devObj){
#ifdef __APPLE__
                    m_wan_bind_enable = false;
#endif
                    return;
                }
                BindInfo*            info = devObj->get_bind_info();
                UnBindMachineDialog dlg;
                dlg.update_device_info2(info);
                dlg.ShowModal();
                /*if (dlg.ShowModal() == wxID_OK) {
                    devOpr->set_selected_machine("");
                }*/
#ifdef __APPLE__
                m_wan_bind_enable = false;
#endif
                });
            string name = devObj->get_dev_name();
            if (!devObj->is_online()) {
                op->SetToolTip(_L("Offline"));
                op->set_printer_state(PrinterState::OFFLINE_WAN);
            }
            else {
                //op->show_edit_printer_name(true);
                op->show_printer_bind(true, PrinterBindState::ALLOW_UNBIND);
                if (devObj->is_in_printing()) {
                    op->SetToolTip(_L("Busy"));
                    op->set_printer_state(PrinterState::BUSY);
                }
                else {
                    op->SetToolTip(_L("Online"));
                    op->set_printer_state(PrinterState::ONLINE_WAN);
                }
            }
        }

        op->Bind(EVT_CONNECT_LAN_PRINT, [this, devObj](wxCommandEvent &e) {
            if (devObj) {
                if (devObj->is_lan_mode_printer()) {
                    //ConnectPrinterDialog dlg(wxGetApp().mainframe, wxID_ANY, _L("Input access code"));
                    ConnectPrinterDialog dlg;
                    dlg.set_device_object(devObj);
                    if (dlg.ShowModal() == wxID_OK) {
                        wxGetApp().mainframe->jump_to_monitor(devObj->get_dev_id());
                    }
                }
            }
#ifdef __APPLE__
            m_wan_bind_enable = false;
#endif
        });

         /*op->Bind(EVT_EDIT_PRINT_NAME, [this, devObj](wxCommandEvent &e) {
            EditDevNameDialog dlg;
            dlg.set_machine_obj(mobj);
            dlg.ShowModal();
         });*/
    }

    if (m_updateConnect) {
        m_updateConnect = false;
        BOOST_LOG_TRIVIAL(info) << "update_user_devices--timer stop";
        m_refresh_timer->Stop();
    }

    for (int j = i; j < m_user_list_machine_panel.size(); j++) {
        m_user_list_machine_panel[j]->mPanel->update_device_info(nullptr);
        m_user_list_machine_panel[j]->mPanel->Hide();
    }
    //m_sizer_my_devices->Layout();

    if (m_my_devices_count != i) {
		m_scrolledWindow->Fit();
    }
    m_scrolledWindow->Layout();
    m_scrolledWindow->Thaw();
	Layout();
	Fit();
	this->Thaw();
    m_my_devices_count = i;
}

bool SelectMachinePopup::search_for_printer(MachineObject* obj)
{
	std::string search_text = std::string((m_search_bar->GetValue()).mb_str());
	if (search_text.empty()) {
		return true;
	}
	auto name = obj->get_dev_name();
	auto ip = obj->get_dev_ip();
	auto name_it = name.find(search_text);
	auto ip_it = ip.find(search_text);
	if ((name_it != std::string::npos)||(ip_it != std::string::npos)) {
		return true;
	}

    return false;
}

void SelectMachinePopup::on_dissmiss_win(wxCommandEvent &event)
{
    Dismiss();
}

void SelectMachinePopup::update_machine_list(wxCommandEvent &event)
{
    DeviceObjectOpr *devOpr = wxGetApp().getDeviceObjectOpr();
    devOpr->update_scan_machine();

    update_user_devices();
    update_other_devices();
    BOOST_LOG_TRIVIAL(trace) << "SelectMachinePopup update_machine_list end";
}

void SelectMachinePopup::on_dclick_up(wxMouseEvent &event)
{
    //BOOST_LOG_TRIVIAL(info) << "SelectMachinePopup on_dclick_up";
    //flush_logs();
    //event.Skip();
}

void SelectMachinePopup::OnLeftUp(wxMouseEvent &event)
{
    auto mouse_pos = ClientToScreen(event.GetPosition());
    auto wxscroll_win_pos = m_scrolledWindow->ClientToScreen(wxPoint(0, 0));
#ifdef __APPLE__
    BOOST_LOG_TRIVIAL(info) << "SelectMachinePopup uOnLeftUp";
#endif
    if (mouse_pos.x > wxscroll_win_pos.x && mouse_pos.y > wxscroll_win_pos.y && mouse_pos.x < (wxscroll_win_pos.x + m_scrolledWindow->GetSize().x) &&
        mouse_pos.y < (wxscroll_win_pos.y + m_scrolledWindow->GetSize().y)) {
        for (MachinePanel* p : m_user_list_machine_panel) {
            auto p_rect = p->mPanel->ClientToScreen(wxPoint(0, 0));
            if (mouse_pos.x > p_rect.x && mouse_pos.y > p_rect.y && mouse_pos.x < (p_rect.x + p->mPanel->GetSize().x) && mouse_pos.y < (p_rect.y + p->mPanel->GetSize().y)) {
#ifdef __APPLE__
                if(!p->mPanel->device_info())
                    break;
#endif
                wxMouseEvent event(wxEVT_LEFT_UP);
                auto         tag_pos = p->mPanel->ScreenToClient(mouse_pos);
                event.SetPosition(tag_pos);
                event.SetEventObject(p->mPanel);
                wxPostEvent(p->mPanel, event);
#ifdef __APPLE__
                break;
#endif
            }
        }

        for (MachinePanel* p : m_other_list_machine_panel) {
            auto p_rect = p->mPanel->ClientToScreen(wxPoint(0, 0));
            if (mouse_pos.x > p_rect.x && mouse_pos.y > p_rect.y && mouse_pos.x < (p_rect.x + p->mPanel->GetSize().x) && mouse_pos.y < (p_rect.y + p->mPanel->GetSize().y)) {
#ifdef __APPLE__
                if(!p->mPanel->device_info())
                    break;
                BOOST_LOG_TRIVIAL(info) << "SelectMachinePopup uOnLeftUp---" << p->mPanel->device_info()->get_dev_name();
#endif
                wxMouseEvent event(wxEVT_LEFT_UP);
                auto         tag_pos = p->mPanel->ScreenToClient(mouse_pos);
                event.SetPosition(tag_pos);
                event.SetEventObject(p->mPanel);
                wxPostEvent(p->mPanel, event);
#ifdef __APPLE__
                break;
#endif
            }
        }
        flush_logs();
        //hyper link
        /*auto h_rect = m_hyperlink->ClientToScreen(wxPoint(0, 0));
        if (mouse_pos.x > h_rect.x && mouse_pos.y > h_rect.y && mouse_pos.x < (h_rect.x + m_hyperlink->GetSize().x) && mouse_pos.y < (h_rect.y + m_hyperlink->GetSize().y)) {
          wxLaunchDefaultBrowser(wxT("https://wiki.bambulab.com/en/software/bambu-studio/failed-to-connect-printer"));
        }*/
    }
}

void SelectMachinePopup::ProcessLeftDown(const wxPoint& pnt)
{
#if 0
    auto rect = m_scrolledWindow->GetRect();
    printf("rect: %d, %d, %d, %d; pnt:%d, %d\n", rect.x, rect.y, rect.width, rect.height, pnt.x, pnt.y);
    rect.x = rect.x + rect.width - 10;
    rect.width = 8;
    if(rect.Contains(pnt)){
        m_left_down = true;
        m_mouse_pos = pnt;
        m_scroll_pos_start.y = m_scrolledWindow->GetScrollPos(wxVERTICAL);
    }
#endif
    return;
    //BOOST_LOG_TRIVIAL(info) << "DeviceFilterPopupWindow::ProcessLeftDown";
    //flush_logs();
#if 0
    m_last_point = pnt;
    for (auto& it : m_items) {
        it->SetPressed(false, false);
    }
    for (auto& it : m_items) {
        if (it->GetRect().Contains(pnt) ){
            it->SetPressed(true, true);
            break;
        }
    }
#endif
}

void SelectMachinePopup::ProcessLeftUp(const wxPoint& pnt)
{
#if 0
    for (auto& it : m_items) {
        if (it->IsPressed() && it->GetRect().Contains(pnt)) {
            it->SetPressed(false, true);
            break;
        } else {
            it->SetPressed(false, false);
        }
    }
#endif
#if 0
    int deltay = pnt.y - m_mouse_pos.y;
    auto rect = m_scrolledWindow->GetRect();
    rect.x = rect.x + rect.width - 10;
    rect.width = 8;
    if(m_left_down && rect.Contains(pnt)){
        m_left_down = false;
        int y = m_scrolledWindow->GetScrollPos(wxVERTICAL);
        wxPoint newPos(m_scroll_pos_start.x, pnt.y);
        m_scrolledWindow->Scroll(newPos);
        //m_scrolledWindow->SetScrollPos(wxVERTICAL, newPos.y);
        Refresh();
        return;
    }
    m_left_down = false;
#endif
    for (MachinePanel* p : m_user_list_machine_panel) {
        if (/*p->mPanel->IsPressed() &&*/ p->mPanel->GetRect().Contains(pnt)) {
#ifdef __APPLE__
            if(!p->mPanel->device_info())
                break;
#endif
            p->mPanel->SetPressed(false, true);
            wxMouseEvent event(wxEVT_LEFT_UP);
            auto tag_pos = wxPoint(pnt.x, pnt.y - p->mPanel->GetPosition().y);
            event.SetPosition(tag_pos);
            event.SetEventObject(p->mPanel);
            wxPostEvent(p->mPanel, event);
#ifdef __APPLE__
            Dismiss();
            return;
#endif
        } else {
            p->mPanel->SetPressed(false, false);
        }
    }
    
    for (MachinePanel* p : m_other_list_machine_panel) {
        if (/*p->mPanel->IsPressed() &&*/ p->mPanel->GetRect().Contains(pnt)) {
#ifdef __APPLE__
            if(!p->mPanel->device_info())
                break;
#endif
            p->mPanel->SetPressed(false, true);
            wxMouseEvent event(wxEVT_LEFT_UP);
            auto tag_pos = wxPoint(pnt.x, pnt.y - p->mPanel->GetPosition().y);
            event.SetPosition(tag_pos);
            event.SetEventObject(p->mPanel);
            wxPostEvent(p->mPanel, event);
#ifdef __APPLE__
            Dismiss();
            return;
#endif
        } else {
            p->mPanel->SetPressed(false, false);
        }
    }
    //Dismiss();
}

void SelectMachinePopup::ProcessMotion(const wxPoint& pnt)
{
#if 0
    for (auto& it : m_items) {
        it->SetHover(false);
    }
    for (auto& it : m_items) {
        if (it->GetRect().Contains(pnt)) {
            it->SetHover(true);
            break;
        }
    }
#endif
#if 0
    int deltay = pnt.y - m_mouse_pos.y;
    auto rect = m_scrolledWindow->GetRect();
    rect.x = rect.x + rect.width - 10;
    rect.width = 8;
    if(m_left_down && rect.Contains(pnt)){
        wxPoint newPos(m_scroll_pos_start.x, m_scroll_pos_start.y + deltay);
        m_scrolledWindow->Scroll(newPos);
        //m_scrolledWindow->SetScrollPos(wxVERTICAL, newPos.y);
        m_mouse_pos = pnt;
        m_scroll_pos_start.y = newPos.y;
        Refresh();
        return;
    }
#endif
    for (MachinePanel* p : m_user_list_machine_panel) {
        p->mPanel->SetHover(false);
    }
    for (MachinePanel* p : m_other_list_machine_panel) {
        p->mPanel->SetHover(false);
    }
    for (MachinePanel* p : m_user_list_machine_panel) {
        if(p->mPanel->GetRect().Contains(pnt)) {
            p->mPanel->SetHover(true);
            break;
        }
    }
    for (MachinePanel* p : m_other_list_machine_panel) {
        if(p->mPanel->GetRect().Contains(pnt)){
            p->mPanel->SetHover(true);
            break;
        }
    }
}
#endif
static wxString MACHINE_BED_TYPE_STRING[BED_TYPE_COUNT] = {
    //_L("Auto"),
    _L("Bambu Cool Plate") + " / " + _L("PLA Plate"),
    _L("Bambu Engineering Plate"),
    _L("Bambu Smooth PEI Plate") + "/" + _L("High temperature Plate"),
    _L("Bambu Textured PEI Plate")};

static std::string MachineBedTypeString[BED_TYPE_COUNT] = {
    //"auto",
    "pc",
    "pe",
    "pei",
    "pte",
};

 }} // namespace Slic3r::GUI
