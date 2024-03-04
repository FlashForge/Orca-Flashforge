#include "DeviceListPanel.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/FFUtils.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/Widgets/FFToggleButton.hpp"
#include "DeviceData.hpp"

namespace Slic3r {
namespace GUI {

CustomComboBox::CustomComboBox(wxWindow* parent, const wxString& name)
    : wxPanel(parent)
    , m_name(name)
{
    initControl();
    setCustomBoxSizer();
}

void CustomComboBox::initControl()
{
    m_staticText_name = new wxStaticText(this, wxID_ANY, m_name);
    m_staticBitmap = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0);
    m_staticBitmap->SetBitmap(create_scaled_bitmap("custom_comboBox_down", nullptr, 10));
}

void CustomComboBox::setCustomBoxSizer()
{
    wxBoxSizer *hTopBoxSizer = new wxBoxSizer(wxLI_HORIZONTAL);
    hTopBoxSizer->Add(m_staticText_name, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    hTopBoxSizer->AddSpacer(20);
    hTopBoxSizer->Add(m_staticBitmap, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    SetSizer(hTopBoxSizer);
    Layout();
    Fit();
}

ListBoxPopup::ListBoxPopup(wxWindow *parent, const wxArrayString &names)
    : PopupWindow(parent, wxBORDER_NONE | wxPU_CONTAINS_CONTROLS), m_dismiss(false)
{
    m_listBox = new wxListBox(this, wxID_ANY);
    m_listBox->InsertItems(names, m_listBox->GetCount());
    m_listBox->SetSelection(0);

    wxBoxSizer *vSizer = new wxBoxSizer(wxVERTICAL);
    vSizer->Add(m_listBox, 0, wxALL);
    SetSizer(vSizer);
    Layout();
    Fit();
}

void ListBoxPopup::Popup(wxWindow *WXUNUSED(focus))
{
    PopupWindow::Popup();
}

void ListBoxPopup::OnDismiss()
{

}

bool ListBoxPopup::ProcessLeftDown(wxMouseEvent &event)
{
    return PopupWindow::ProcessLeftDown(event);
}

bool ListBoxPopup::Show(bool show)
{
    return PopupWindow::Show(show);
}

void ListBoxPopup::resetSize(const wxSize &size)
{
    m_listBox->SetSize(size.x, m_listBox->GetCount() * LISTBOX_HEIGHT);
    SetSize(size.x, GetSize().y);
    Fit();
}


std::map<unsigned short, wxBitmap> DeviceItemPanel::m_machineBitmapMap;
DeviceItemPanel::DeviceItemPanel(wxWindow *parent, const DeviceInfo& info) 
    : wxPanel(parent)
{ 
    build();
    connectEvent();
    updateInfo(info);
}

void DeviceItemPanel::build()
{
    Freeze();
    SetSize(FromDIP(220), FromDIP(205));
    SetMinSize(wxSize(FromDIP(220), FromDIP(205)));
    SetMaxSize(wxSize(FromDIP(220), FromDIP(205)));

    m_name_text = new wxStaticText(this, wxID_ANY, wxT("AD5M"));
    m_name_text->SetBackgroundColour(m_bg_color);
    m_icon = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize(FromDIP(112), FromDIP(112)), wxALIGN_LEFT);
    m_icon->SetMinSize(wxSize(FromDIP(112), FromDIP(112)));
    m_icon->SetMaxSize(wxSize(FromDIP(112), FromDIP(112)));
    m_icon->SetBackgroundColour(m_bg_color);
    //m_icon->SetBitmap(create_scaled_bitmap("device_Ad5m", nullptr, FromDIP(112)));
    m_placement_text = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_placement_text->SetBackgroundColour(m_bg_color);
    m_status_text = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_status_text->SetBackgroundColour(m_bg_color);

    m_main_sizer = new wxBoxSizer(wxVERTICAL);
    m_main_sizer->AddStretchSpacer(1);
    m_main_sizer->Add(m_name_text, 0, wxEXPAND | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(10));
    m_main_sizer->AddSpacer(FromDIP(3));
    m_main_sizer->Add(m_icon, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, FromDIP(10));
    m_main_sizer->AddSpacer(FromDIP(3));
    m_main_sizer->Add(m_placement_text, 0, wxEXPAND | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(10));
    m_main_sizer->AddSpacer(FromDIP(3));
    m_main_sizer->Add(m_status_text, 0, wxEXPAND | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(10));
    m_main_sizer->AddStretchSpacer(1);

    SetSizer(m_main_sizer);
    Layout();
    Fit();
    Thaw();
}

void DeviceItemPanel::connectEvent() 
{
    Bind(wxEVT_LEFT_DOWN, &DeviceItemPanel::mouseDown, this);
    Bind(wxEVT_LEFT_UP, &DeviceItemPanel::mouseReleased, this);
    Bind(wxEVT_ENTER_WINDOW, &DeviceItemPanel::onEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &DeviceItemPanel::onLeave, this);
    Bind(wxEVT_PAINT, &DeviceItemPanel::onPaint, this);
}

bool DeviceItemPanel::isPointIn(const wxPoint& pt)
{
    return !(wxHT_WINDOW_OUTSIDE == HitTest(pt));
}

void DeviceItemPanel::mouseDown(wxMouseEvent& event)
{
    if (isPointIn(event.GetPosition())) {
        m_pressed = true;
        Refresh();
    }
    event.Skip();
}

void DeviceItemPanel::mouseReleased(wxMouseEvent& event)
{
    if (isPointIn(event.GetPosition())) {
        sendEvent();
    }
    m_pressed = false;
    Refresh();
    event.Skip();
}

void DeviceItemPanel::onEnter(wxMouseEvent& event)
{
    if (isPointIn(event.GetPosition())) {
        m_hovered = true;
        Refresh();
        CaptureMouse();
    }
    event.Skip();
}

void DeviceItemPanel::onLeave(wxMouseEvent& event)
{
    if (!isPointIn(event.GetPosition())) {
        m_hovered = false;
        m_pressed = false;
        Refresh();
        if (HasCapture()) {
            ReleaseMouse(); 
        }
    }
    event.Skip();
}

void DeviceItemPanel::onPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    wxSize sz = GetSize();
    wxPen pen;
    pen.SetWidth(3);
    dc.SetBrush(m_bg_color);
    if (m_hovered && m_pressed) {
        pen.SetColour(m_border_press_color);
    } else if (m_hovered) {
        pen.SetColour(m_border_hover_color);
    } else {
        pen.SetColour(m_border_color);
    }
    dc.SetPen(pen);
    dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, 7);
    event.Skip();
}

void DeviceItemPanel::sendEvent()
{
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, GetId());
    event.SetEventObject(this);
    wxPostEvent(this, event);
}

void DeviceItemPanel::updateInfo(const DeviceInfo& info)
{
    m_name_text->SetLabel(info.name);
    if (info.placement.empty()) {
        m_placement_text->SetLabel(_L("Default"));
    } else {
        m_placement_text->SetLabel(info.placement);
    }
    if (info.pid != m_info.pid) {
        m_icon->SetBitmap(machineBitmap(info.pid));
    }
    m_info = info;
    updateStatus();
    Layout();
}

const DeviceItemPanel::DeviceInfo& DeviceItemPanel::deviceInfo() const
{
    return m_info;
}

wxBitmap DeviceItemPanel::machineBitmap(unsigned short pid)
{
    wxBitmap bmp;
    auto iter = m_machineBitmapMap.find(pid);
    if (iter == m_machineBitmapMap.end()) {
        wxString bmp_str = FFUtils::getBitmapFileName(pid);
        if (!bmp_str.empty()) {
            bmp = create_scaled_bitmap(bmp_str.ToStdString(), 0, 120);
            m_machineBitmapMap[pid] = bmp;
        }
    } else {
        bmp = iter->second;
    }
    return bmp;
}

void DeviceItemPanel::updateStatus()
{
    wxString status = _T("Idle");    
    wxColour color("#00CD6D");
    if (m_info.conn_id < 0) {
        status = _L("Offline");
        color = wxColour("#999999");
    } else {
        wxString rawstatus = m_info.status;
        if ("printing" == rawstatus) {
            status = _L("Printing");
            color = wxColour("#4D54FF");
        } else if ("pause" == rawstatus || "pausing" == rawstatus) {
            status = _L("Paused");
            color = wxColour("#982187");
        } else if ("error" == rawstatus) {
            status = _L("Error");
            color = wxColour("#FD4A29");
        } else if ("busy" == rawstatus || "calibrate_doing" == rawstatus || "heating" == rawstatus) {
            status = _L("Busy");
            color = wxColour("#F9B61C");
        } else if ("completed" == rawstatus) {
            status = _L("Completed");
            color = wxColour("#328DFB");
        } else if ("cancel" == rawstatus || "canceling" == rawstatus) {
            status = _L("Cancel");
            color = wxColour("#328DFB");
        }
        //} else if ("heating" == rawstatus) {
        //    status = _L("Heating");
        //    //color = wxColour("");
        //}
    }
    m_status_text->SetLabel(status);
    m_status_text->SetForegroundColour(color);
}


DeviceListPanel::DeviceListPanel(wxWindow* parent, wxWindowID id, const wxPoint& pos, 
        const wxSize& size, long style, const wxString& name)
    : wxPanel(parent, id, pos, size, style, name)
{
    this->SetBackgroundColour(0xEEEEEE);

    build();
    connectEvent();
    initDeviceList();
}

DeviceListPanel::~DeviceListPanel()
{
    
}

void DeviceListPanel::msw_rescale()
{
	
}

void DeviceListPanel::build()
{
    SetBackgroundColour(wxColour("#F0F0F0"));
    m_comboBox_position = new CustomComboBox(this, _L("Position"));
    wxArrayString names;
    names.Add("All");
    names.Add("Offen");
    names.Add("Store1");
    names.Add("Store2");
    m_listBox_position = new ListBoxPopup(this, names);
    m_comboBox_status  = new CustomComboBox(this, _L("Status"));
    initAllDeviceStatus(names);
    m_listBox_status = new ListBoxPopup(this, names);
    m_comboBox_type = new CustomComboBox(this, _L("Machine Type"));
    m_wlan_btn = new FFToggleButton(this, _L("Network"));
    m_wlan_btn->SetBackgroundColour(GetBackgroundColour());
    m_wlan_btn->SetValue(true);
    m_lan_btn = new FFToggleButton(this, _L("LAN"));
    m_lan_btn->SetBackgroundColour(GetBackgroundColour());
    m_lan_btn->SetValue(true);
    m_static_btn = new wxToggleButton(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(16), FromDIP(16)));
    m_static_btn->SetValue(false);
    m_static_btn->SetMinSize(wxSize(FromDIP(16), FromDIP(16)));
    m_static_btn->SetMaxSize(wxSize(FromDIP(16), FromDIP(16)));
    m_static_btn->SetBitmap(create_scaled_bitmap("device_more", this, FromDIP(16)));
    m_static_btn->SetBitmapHover(create_scaled_bitmap("device_more_hover", this, FromDIP(16)));
    m_static_btn->SetBitmapPressed(create_scaled_bitmap("device_more_press", this, FromDIP(16)));

    wxBoxSizer* hTopSizer = new wxBoxSizer(wxHORIZONTAL);
    hTopSizer->AddSpacer(20);
    hTopSizer->Add(m_comboBox_position, 0, wxALIGN_CENTER_VERTICAL);
    hTopSizer->AddSpacer(50);
    hTopSizer->Add(m_comboBox_status, 0, wxALIGN_CENTER_VERTICAL);
    hTopSizer->AddSpacer(50);
    hTopSizer->Add(m_comboBox_type, 0, wxALIGN_CENTER_VERTICAL);
    hTopSizer->AddStretchSpacer(1);
    hTopSizer->Add(m_wlan_btn, 0, wxALIGN_CENTER_VERTICAL);
    hTopSizer->AddSpacer(10);
    hTopSizer->Add(new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxSize(1, 28), wxLI_VERTICAL), 0, wxALIGN_CENTER_VERTICAL, 10);
    hTopSizer->AddSpacer(10);
    hTopSizer->Add(m_lan_btn, 0, wxALIGN_CENTER_VERTICAL);
    hTopSizer->AddSpacer(30);
    hTopSizer->Add(m_static_btn, 0, wxALIGN_CENTER_VERTICAL);
    hTopSizer->AddSpacer(20);

    m_simple_book = new wxSimplebook(this);
    // no device panel
    m_no_device_panel = new wxPanel(m_simple_book);
    m_no_device_bitmap = new wxStaticBitmap(m_no_device_panel, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0);
    m_no_device_bitmap->SetBitmap(create_scaled_bitmap("monitor_device_empty", nullptr, 250));
    m_no_device_staticText = new wxStaticText(m_no_device_panel, wxID_ANY, wxT("No Device"));
    m_no_device_staticText->Wrap(-1);
    m_no_device_staticText->SetForegroundColour(0x909090);
    m_no_device_sizer = new wxBoxSizer(wxVERTICAL);
    m_no_device_sizer->AddStretchSpacer();
    m_no_device_sizer->Add(m_no_device_bitmap, 0, wxALIGN_CENTER_HORIZONTAL);
    m_no_device_sizer->AddSpacer(20);
    m_no_device_sizer->Add(m_no_device_staticText, 1, wxALL | wxALIGN_CENTER_HORIZONTAL);
    m_no_device_sizer->AddStretchSpacer();
    m_no_device_panel->SetSizer(m_no_device_sizer);
    m_no_device_panel->Layout();
    m_simple_book->AddPage(m_no_device_panel, wxEmptyString, true);

    m_device_window = new wxScrolledWindow(m_simple_book);
    m_device_window->EnableScrolling(true, true);
    m_device_window->SetScrollRate(0, 10);
    wxBoxSizer* device_sizer = new wxBoxSizer(wxVERTICAL);
    //m_device_panel->SetBackgroundColour(wxColour("#F0F0F0"));
    m_device_sizer = new wxGridSizer(5);
    m_device_sizer->SetHGap(FromDIP(40));
    m_device_sizer->SetVGap(FromDIP(40));
    device_sizer->Add(m_device_sizer, 0, wxALIGN_LEFT | wxALIGN_TOP | wxALL, FromDIP(40));
    m_device_window->SetSizer(device_sizer);
    m_simple_book->AddPage(m_device_window, wxEmptyString, false);

    wxStaticLine *horLine = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxSize(160, 1), wxLI_HORIZONTAL);
    horLine->SetBackgroundColour(wxColour(255, 255, 255, 0));
    wxBoxSizer *vAllSizer = new wxBoxSizer(wxVERTICAL);
    vAllSizer->Add(hTopSizer, 0, wxEXPAND);
    vAllSizer->Add(horLine, 0, wxALIGN_CENTER_HORIZONTAL | wxEXPAND, 10);
    vAllSizer->Add(m_simple_book, 1, wxEXPAND);
    //vAllSizer->Add(m_machinePanel, 1, wxEXPAND);

    this->SetSizer(vAllSizer);
    this->Layout();
    this->Fit();
}

void DeviceListPanel::initAllDeviceStatus(wxArrayString &names)
{
    names.Clear();
    names.Add("All");
    names.Add("Idle");
    names.Add("Busy");
    names.Add("Error");
    names.Add("Printing");
}

void DeviceListPanel::connectEvent()
{
    m_comboBox_position->Bind(wxEVT_LEFT_DOWN, &DeviceListPanel::on_comboBox_position_clicked, this);
    m_comboBox_status->Bind(wxEVT_LEFT_DOWN, &DeviceListPanel::on_comboBox_status_clicked, this);
    m_wlan_btn->Bind(wxEVT_TOGGLEBUTTON, &DeviceListPanel::onNetworkTypeToggled, this);
    m_lan_btn->Bind(wxEVT_TOGGLEBUTTON, &DeviceListPanel::onNetworkTypeToggled, this);
    m_static_btn->Bind(wxEVT_TOGGLEBUTTON, &DeviceListPanel::on_static_mode_toggled, this);
    MultiComMgr::inst()->Bind(COM_DEV_DETAIL_UPDATE_EVENT, &DeviceListPanel::onComDevDetailUpdate, this);
    wxGetApp().getDeviceObjectOpr()->Bind(EVT_DEVICE_LIST_UPDATED, &DeviceListPanel::onDeviceListUpdated, this);
}

void DeviceListPanel::initLocalDevice(std::map<std::string, DeviceItemPanel::DeviceInfo>& deviceInfoMap)
{
    deviceInfoMap.clear();
    AppConfig *config = wxGetApp().app_config;
    if (config) {
        std::vector<MacInfoMap> macInfo;
        config->get_local_mahcines(macInfo);
        DeviceItemPanel::DeviceInfo dev_info;
        for (auto& mac : macInfo) {
            auto it = mac.find("dev_id");
            if (it != mac.end()) {
                std::string dev_id = it->second;
                it = mac.find("dev_name");
                if (it != mac.end()) {
                    dev_info.name = it->second;
                }
                it = mac.find("dev_placement");
                if (it != mac.end()) {
                    dev_info.placement = it->second;
                }
                it = mac.find("dev_pid");
                if (it != mac.end()) {
                    dev_info.pid = (unsigned short)std::stoi(it->second);
                }
                if (deviceInfoMap.find(dev_id) == deviceInfoMap.end()) {
                    deviceInfoMap.emplace(dev_id, dev_info);
                }
            }
        }
    }
}

void DeviceListPanel::initDeviceList()
{
    std::map<std::string, DeviceItemPanel::DeviceInfo> devList;
    initLocalDevice(devList);
    if (devList.empty()) {
        m_simple_book->ChangeSelection(0);
        return;
    } else {
        m_simple_book->ChangeSelection(1);
    }

    std::vector<std::string> devKeyList;
    for (auto it: devList) {
        devKeyList.emplace_back(it.first);
    }
    std::sort(devKeyList.begin(), devKeyList.end(), [&devList](auto& a, auto&b) {
        return devList[a].name.compare(devList[b].name) < 0;
    });
    //m_device_map
    for (auto iter : devKeyList) {
        DeviceItemPanel* item = new DeviceItemPanel(m_device_window, devList[iter]);
        m_device_map.emplace(std::make_pair(iter, item));
        m_device_sizer->Add(item);
    }
    m_device_window->Layout();
    Layout();
}

void DeviceListPanel::on_comboBox_position_clicked(wxMouseEvent &event)
{
    auto    mouse_pos = ClientToScreen(event.GetPosition());
    wxPoint rect      = m_comboBox_position->ClientToScreen(wxPoint(0, 0));
    wxPoint pos       = m_comboBox_position->ClientToScreen(wxPoint(0, 0));
    pos.y += m_comboBox_position->GetRect().height;
    m_listBox_position->Move(pos);
    m_listBox_position->resetSize(wxSize(m_comboBox_position->GetSize().x, -1));
    m_listBox_position->Popup();
}

void DeviceListPanel::on_comboBox_status_clicked(wxMouseEvent &event) 
{
    auto    mouse_pos = ClientToScreen(event.GetPosition());
    wxPoint rect      = m_comboBox_status->ClientToScreen(wxPoint(0, 0));
    wxPoint pos       = m_comboBox_status->ClientToScreen(wxPoint(0, 0));
    pos.y += m_comboBox_status->GetRect().height;
    m_listBox_status->Move(pos);
    m_listBox_status->resetSize(wxSize(m_comboBox_status->GetSize().x, -1));
    m_listBox_status->Popup();
}

void DeviceListPanel::onNetworkTypeToggled(wxCommandEvent& event)
{
    
}

void DeviceListPanel::on_static_mode_toggled(wxCommandEvent &event)
{
    m_simple_book->SetSelection(1);
    m_simple_book->Layout();
    Layout();
}

void DeviceListPanel::onDeviceListUpdated(DeviceListUpdateEvent& event)
{
    std::string dev_id = event.GetDeviceId();
    int conn_id = event.GetConnectionId();
    bool valid = false;
    const com_dev_data_t &data      = MultiComMgr::inst()->devData(conn_id, &valid);
    if (valid) {
        DeviceItemPanel::DeviceInfo dev_info;
        dev_info.conn_id = conn_id;
        dev_info.lanFlag = (COM_CONNECT_LAN == data.connectMode);
        dev_info.name = data.devDetail->name;
        dev_info.pid = data.devDetail->pid;
        dev_info.placement = data.devDetail->location;
        dev_info.status = data.devDetail->status;

        //
        auto iter = m_device_map.find(dev_id);
        if (iter == m_device_map.end()) {
            DeviceItemPanel* item = new DeviceItemPanel(m_device_window, dev_info);
            m_device_map.emplace(std::make_pair(dev_id, item));
            m_device_sizer->Add(item);
            m_device_window->Layout();
            Layout();
        } else {
            //if (data.devDetail->status)
        }
    } else {
        auto iter = m_device_map.find(dev_id);
        if (iter != m_device_map.end()) {
            m_device_sizer->Detach(iter->second);
            iter->second->Destroy();
            m_device_map.erase(iter);            
            m_device_window->Layout();
            Layout();
        }
    }

    event.Skip();

}

void DeviceListPanel::onComDevDetailUpdate(ComDevDetailUpdateEvent& event)
{
    auto com_id = event.id;
    bool valid = false;
    auto data = MultiComMgr::inst()->devData(com_id, &valid);
    if (valid) {
        std::string dev_id = (data.connectMode == COM_CONNECT_LAN) ? data.lanDevInfo.serialNumber : data.wanDevInfo.serialNumber;
        auto iter = m_device_map.find(dev_id);
        if (iter != m_device_map.end()) {
            auto dev_info = iter->second->deviceInfo();
            dev_info.name = data.devDetail->name;
            dev_info.pid = data.devDetail->pid;
            dev_info.placement = data.devDetail->location;
            dev_info.status = data.devDetail->status;
            iter->second->updateInfo(dev_info);
        }
    }
    event.Skip();
}

} // GUI
} // Slic3r