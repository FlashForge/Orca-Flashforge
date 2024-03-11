#include "DeviceListPanel.hpp"
#include <boost/log/trivial.hpp>
#include <wx/graphics.h>
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/FFUtils.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/Widgets/FFToggleButton.hpp"
#include "slic3r/GUI/Widgets/FFCheckBox.hpp"
#include "slic3r/GUI/wxExtensions.hpp"
#include "slic3r/GUI/FFUtils.hpp"
#include "DeviceData.hpp"

namespace Slic3r {
namespace GUI {

DropDownButton::DropDownButton(wxWindow* parent/*=nullptr*/, const wxString& name/*=wxEmptyString*/, const wxBitmap& bitmap/*=wxNullBitmap*/)
    : wxPanel(parent)
{
    if (parent) {
        SetBackgroundColour(parent->GetBackgroundColour());
    }
    m_text = new wxStaticText(this, wxID_ANY, name, wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    m_bitmap = new wxStaticBitmap(this, wxID_ANY, bitmap, wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    if (!bitmap.IsNull()) {
        auto sz = bitmap.GetSize();
        m_bitmap->SetSize(bitmap.GetSize());
        m_bitmap->SetMinSize(bitmap.GetSize());
    }

    m_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_sizer->AddStretchSpacer(1);
    m_sizer->Add(m_text, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, FromDIP(3));
    m_sizer->AddSpacer(FromDIP(12));
    m_sizer->Add(m_bitmap, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, FromDIP(3));
    m_sizer->AddStretchSpacer(1);

    SetSizer(m_sizer);
    Layout();
    Fit();
    //initControl();
    //setCustomBoxSizer();

    Bind(wxEVT_ENTER_WINDOW, &DropDownButton::onEnter, this);
	Bind(wxEVT_LEAVE_WINDOW, &DropDownButton::onLeave, this);
    m_text->Bind(wxEVT_ENTER_WINDOW, &DropDownButton::onEnter, this);
    m_bitmap->Bind(wxEVT_ENTER_WINDOW, &DropDownButton::onEnter, this);
}

wxPoint DropDownButton::convertEventPoint(const wxMouseEvent& event)
{
    wxPoint pnt = event.GetPosition();
    if (event.GetId() == m_text->GetId()) {
        pnt += m_text->GetPosition();
    } else if (event.GetId() == m_bitmap->GetId()) {
        pnt += m_bitmap->GetPosition();
    }
    return pnt;
}

void DropDownButton::onEnter(wxMouseEvent& event)
{
    if (isPointIn(convertEventPoint(event))) {
        if (!HasCapture()) {
            //BOOST_LOG_TRIVIAL(error) << "DropDownButton::onEnter";
            //flush_logs();
            CaptureMouse();
        }
    }
    event.Skip();
}

void DropDownButton::onLeave(wxMouseEvent& event)
{
    if (!isPointIn(convertEventPoint(event))) {
        if (HasCapture()) {
            //BOOST_LOG_TRIVIAL(debug) << "DropDownButton::onLeave";
            //flush_logs();
            ReleaseMouse();
        }
    }
    event.Skip();
}

bool DropDownButton::isPointIn(const wxPoint& pnt)
{
    return !(wxHT_WINDOW_OUTSIDE == HitTest(pnt));
}


wxDEFINE_EVENT(EVT_FILTER_ITEM_CLICKED, wxCommandEvent);
FilterPopupWindow::FilterItem::FilterItem(wxWindow* parent, const wxString& text, bool top_corner_round/*=false*/,
    bool bottom_corner_round/*=false*/, bool can_checked/*=false*/)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER)
    , m_topCornerRound(top_corner_round)
    , m_bottomCornerRound(bottom_corner_round)
    , m_can_check(can_checked)
{
    //SetBackgroundColour(wxColour("#eeeeee"));
    SetMinSize(wxSize(FromDIP(80), FromDIP(30)));
    SetMaxSize(wxSize(-1, FromDIP(30)));
    SetSize(wxSize(-1, FromDIP(30)));
    m_check_box = new FFCheckBox(this);
    m_check_box->Show(can_checked);
    m_check_box->SetValue(false);
    m_text = new wxStaticText(this, wxID_ANY, text);
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->AddSpacer(FromDIP(15));
    if (can_checked) {
        sizer->Add(m_check_box, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxTOP | wxBOTTOM, FromDIP(5));
    }
    sizer->Add(m_text, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, FromDIP(5));
    sizer->AddSpacer(FromDIP(15));
    SetSizer(sizer);
    Layout();
    Fit();
    Bind(wxEVT_PAINT, &FilterItem::onPaint, this);
    Bind(wxEVT_ENTER_WINDOW, &FilterItem::onEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &FilterItem::onLeave, this);
    Bind(wxEVT_LEFT_DOWN, &FilterItem::onMouseDown, this);
    Bind(wxEVT_LEFT_UP, &FilterItem::onMouseUp, this);
    m_text->Bind(wxEVT_ENTER_WINDOW, &FilterItem::onEnter, this);
    m_check_box->Bind(wxEVT_ENTER_WINDOW, &FilterItem::onEnter, this);
}

void FilterPopupWindow::FilterItem::setSelect(bool select)
{
    if (m_selectFlag != select) {
        m_selectFlag = select;
        Refresh();
    }    
}

bool FilterPopupWindow::FilterItem::isSelect() const
{
    return m_selectFlag;
}

void FilterPopupWindow::FilterItem::setChecked(bool check)
{
    if (m_check_box->GetValue() != check) {
        m_check_box->SetValue(check);
        Refresh();
    }
}
bool FilterPopupWindow::FilterItem::isChecked() const
{
    return m_check_box->GetValue();
}

wxString FilterPopupWindow::FilterItem::getText() const
{
    return m_text->GetLabel();
}

void FilterPopupWindow::FilterItem::setText(const wxString& text)
{
    m_text->SetLabel(text);
}

void FilterPopupWindow::FilterItem::setTopCornerRound(bool round)
{
    if (m_topCornerRound != round) {
        m_topCornerRound = round;
        Refresh();
    }
}
        
void FilterPopupWindow::FilterItem::setBottomCornerRound(bool round)
{
    if (m_bottomCornerRound != round) {
        m_bottomCornerRound = round;
        Refresh();
    }
}

bool FilterPopupWindow::FilterItem::Show(bool show/*=true*/)
{
    if (m_can_check) {
        m_check_box->Show(show);
    }
    m_text->Show(show);
    return wxPanel::Show(show);
}

void FilterPopupWindow::FilterItem::onPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    auto sz = GetSize();
    dc.SetPen(*wxTRANSPARENT_PEN);
    if (!m_topCornerRound || !m_bottomCornerRound) {
        dc.SetBrush(wxColour("#dddddd"));
        dc.DrawRectangle(0, 0, sz.x, sz.y);
    }
    wxColour color("#ffffff");
    if (m_pressFlag) {
        color = wxColour("#328DFB");
    } else if (m_hoverFlag) {
        color = wxColour("#D9EAFF");
    } else if (m_selectFlag) {
        color = wxColour("#328DFB");
    }
    dc.SetBrush(color);
    if (!m_topCornerRound && !m_bottomCornerRound) {
        dc.DrawRectangle(0, 0, sz.x, sz.y);
    } else if (m_topCornerRound && m_bottomCornerRound) {
        dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, 6);
    } else if (m_topCornerRound) {
        dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, 6);
        dc.DrawRectangle(0, 6, sz.x, sz.y);
    } else if (m_bottomCornerRound) {
        dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, 6);
        dc.DrawRectangle(0, 0, sz.x, 6);
    }
    m_text->SetBackgroundColour(color);
    m_check_box->SetBackgroundColour(color);
}

void FilterPopupWindow::FilterItem::onEnter(wxMouseEvent& event)
{
    wxPoint pnt = event.GetPosition();
    if (event.GetId() == m_text->GetId()) {
        pnt += m_text->GetPosition();
    } else if (event.GetId() == m_check_box->GetId()) {
        pnt += m_check_box->GetPosition();
    }
    if (isPointIn(pnt)) {
        if (!HasCapture()) {
            //BOOST_LOG_TRIVIAL(error) << "FilterItem::onEnter";
            //flush_logs();
            CaptureMouse();
        }
        m_hoverFlag = true;
    }
    Refresh();
    event.Skip();
}

void FilterPopupWindow::FilterItem::onLeave(wxMouseEvent& event)
{
    if (HasCapture()) {
        //BOOST_LOG_TRIVIAL(error) << "FilterItem::onLeave";
        //flush_logs();
        ReleaseMouse();
    }
    m_hoverFlag = false;
    m_pressFlag = false;
    Refresh();
    event.Skip();
}

void FilterPopupWindow::FilterItem::onMouseDown(wxMouseEvent& event)
{
    if (isPointIn(event.GetPosition())) {
        if (m_can_check) {
            m_check_box->SetValue(!m_check_box->GetValue());
            sendEvent();
        }
        m_pressFlag = true;
        Refresh();
    }
    event.Skip();
}

void FilterPopupWindow::FilterItem::onMouseUp(wxMouseEvent& event)
{
    if (isPointIn(event.GetPosition())) {
        m_pressFlag = false;
        if (!m_can_check) {
            sendEvent();
        }
        Refresh();
    }
    event.Skip();
}

void FilterPopupWindow::FilterItem::sendEvent()
{
    wxCommandEvent event(EVT_FILTER_ITEM_CLICKED);
    event.SetEventObject(this);
    event.SetString(m_text->GetLabel());
    wxPostEvent(this, event);
}

//void onMouseUp(wxMouseEvent& event);
bool FilterPopupWindow::FilterItem::isPointIn(const wxPoint& pnt)
{
    return true;
}


FilterPopupWindow::FilterPopupWindow(wxWindow* parent)
    : PopupWindow(parent, wxBORDER_NONE | wxPU_CONTAINS_CONTROLS | wxFRAME_SHAPED)
    , m_sizer(new wxBoxSizer(wxVERTICAL))
{
    //SetBackgroundColour(wxColour("#eeeeee"));
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_sizer, 1, wxEXPAND | wxALL, 1);
    SetSizer(sizer);
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__
    Bind(wxEVT_PAINT, &FilterPopupWindow::onPaint, this);
}

FilterPopupWindow::~FilterPopupWindow()
{
}

void FilterPopupWindow::Create()
{
    //Freeze();
    m_sizer->Clear();
    int max_width = 0;
    int height = m_items.size() * FromDIP(30);
    for (auto btn : m_items) {
        max_width = std::max(btn->GetSize().x, max_width);
    }

    for (auto btn : m_items) {
        btn->SetSize(max_width, FromDIP(30));
        btn->Layout();
        m_sizer->Add(btn, 0, wxEXPAND);
    }
    SetSize(wxSize(max_width+2, height+2));
    Layout();
    //Fit();
    //Thaw();
    Refresh();
}

void FilterPopupWindow::Popup(wxWindow* focus/*=nullptr*/)
{
    Create();
    wxGraphicsPath path = wxGraphicsRenderer::GetDefaultRenderer()->CreatePath();
    wxSize size = GetSize();
    path.AddRoundedRectangle(0, 0, size.x, size.y, 6);
    SetShape(path);
    PopupWindow::Popup();
}

bool FilterPopupWindow::ProcessLeftDown(wxMouseEvent &event)
{
    return PopupWindow::ProcessLeftDown(event);
}

void FilterPopupWindow::AddItem(FilterItem* item)
{
    bool ret = item->Show(true);
    item->Raise();
    m_items.emplace_back(item);
}

void FilterPopupWindow::ClearItem()
{
    bool ret;
    for (auto& iter : m_items) {
        ret = iter->Show(false);
    }
    m_items.clear();
    m_sizer->Clear();
}

void FilterPopupWindow::onPaint(wxPaintEvent& event)
{
    auto sz = GetSize();
    wxPaintDC dc(this);
    dc.SetBrush(wxColour("#dddddd"));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(0, 0, sz.x, sz.y);
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
    blockMouseEvent(false);
    //Bind(wxEVT_LEFT_DOWN, &DeviceItemPanel::mouseDown, this);
    //Bind(wxEVT_LEFT_UP, &DeviceItemPanel::mouseReleased, this);
    //Bind(wxEVT_ENTER_WINDOW, &DeviceItemPanel::onEnter, this);
    //Bind(wxEVT_LEAVE_WINDOW, &DeviceItemPanel::onLeave, this);
    //Bind(wxEVT_MOTION, &DeviceItemPanel::onMotion, this);
    //Bind(wxEVT_MOUSE_CAPTURE_LOST, &DeviceItemPanel::onMouseCaptureLost, this);
    Bind(wxEVT_PAINT, &DeviceItemPanel::onPaint, this);
}

void DeviceItemPanel::blockMouseEvent(bool block)
{
    m_blockFlag = block;
    if (block) {
        Unbind(wxEVT_LEFT_DOWN, &DeviceItemPanel::mouseDown, this);
        Unbind(wxEVT_LEFT_UP, &DeviceItemPanel::mouseReleased, this);
        Unbind(wxEVT_ENTER_WINDOW, &DeviceItemPanel::onEnter, this);
        Unbind(wxEVT_LEAVE_WINDOW, &DeviceItemPanel::onLeave, this);
    } else {
        Bind(wxEVT_LEFT_DOWN, &DeviceItemPanel::mouseDown, this);
        Bind(wxEVT_LEFT_UP, &DeviceItemPanel::mouseReleased, this);
        Bind(wxEVT_ENTER_WINDOW, &DeviceItemPanel::onEnter, this);
        Bind(wxEVT_LEAVE_WINDOW, &DeviceItemPanel::onLeave, this);
    }
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
        if (!HasCapture()) {
            //BOOST_LOG_TRIVIAL(error) << "DeviceItemPanel::onEnter";
            //flush_logs();
            CaptureMouse();
        }
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
            //BOOST_LOG_TRIVIAL(error) << "DeviceIemPanel::onLeave";
            //flush_logs();
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
    if (m_info.conn_id < 0 || "offline" == m_info.status) {
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
    m_filter_popup = new FilterPopupWindow(this);
    m_default_filter_item = new FilterPopupWindow::FilterItem(m_filter_popup, _L("All"), true);
    m_default_filter_item->Bind(EVT_FILTER_ITEM_CLICKED, &DeviceListPanel::onFilterItemClicked, this);
    m_default_filter_item->Show(false);
    SetBackgroundColour(wxColour("#F0F0F0"));
    m_placement_btn = new DropDownButton(this, _L("Position"), create_scaled_bitmap("device_dropdown", this, 8));
    //m_listBox_position = new ListBoxPopup(this, names);
    m_status_btn  = new DropDownButton(this, _L("Status"), create_scaled_bitmap("device_dropdown", this, 8));
    //initAllDeviceStatus(names);
    //m_status = new ListBoxPopup(this, names);
    m_type_btn = new DropDownButton(this, _L("Machine Type"), create_scaled_bitmap("device_dropdown", this, 8));
    m_wlan_btn = new FFToggleButton(this, _L("Network"));
    m_wlan_btn->SetValue(true);
    m_lan_btn = new FFToggleButton(this, _L("LAN"));
    m_lan_btn->SetValue(true);
    m_static_btn = new FFBitmapToggleButton(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(16), FromDIP(16)));
    m_static_btn->setNormalBitmap(create_scaled_bitmap("device_more", this, 16));
    m_static_btn->setNormalHoverBitmap(create_scaled_bitmap("device_more_hover", this, 16));
    m_static_btn->setNormalPressBitmap(create_scaled_bitmap("device_more_press", this, 16));
    m_static_btn->setSelectBitmap(create_scaled_bitmap("device_back", this, 16));
    m_static_btn->setSelectHoverBitmap(create_scaled_bitmap("device_back_hover", this, 16));
    m_static_btn->setSelectPressBitmap(create_scaled_bitmap("device_back_press", this, 16));
    m_static_btn->SetValue(false);

    wxPanel *lan_line = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(1, -1));
    lan_line->SetBackgroundColour("#666666");

    wxBoxSizer* hTopSizer = new wxBoxSizer(wxHORIZONTAL);
    hTopSizer->Add(m_placement_btn, 0, wxALIGN_CENTER_VERTICAL);
    hTopSizer->AddSpacer(FromDIP(50));
    hTopSizer->Add(m_status_btn, 0, wxALIGN_CENTER_VERTICAL);
    hTopSizer->AddSpacer(FromDIP(50));
    hTopSizer->Add(m_type_btn, 0, wxALIGN_CENTER_VERTICAL);
    hTopSizer->AddStretchSpacer(1);
    hTopSizer->Add(m_wlan_btn, 0, wxALIGN_CENTER_VERTICAL);
    hTopSizer->AddSpacer(FromDIP(10));
    hTopSizer->Add(lan_line, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 3);
    hTopSizer->AddSpacer(FromDIP(10));
    hTopSizer->Add(m_lan_btn, 0, wxALIGN_CENTER_VERTICAL);
    hTopSizer->AddSpacer(FromDIP(30));
    hTopSizer->Add(m_static_btn, 0, wxALIGN_CENTER_VERTICAL);

    m_simple_book = new wxSimplebook(this);
    // no device panel
    m_no_device_panel = new wxPanel(m_simple_book);
    m_no_device_bitmap = new wxStaticBitmap(m_no_device_panel, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0);
    m_no_device_bitmap->SetBitmap(create_scaled_bitmap("monitor_device_empty", nullptr, 250));
    m_no_device_staticText = new wxStaticText(m_no_device_panel, wxID_ANY, wxT("No Device"));
    m_no_device_staticText->Wrap(-1);
    m_no_device_staticText->SetForegroundColour("#909090");
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
    device_sizer->Add(m_device_sizer, 0, wxALIGN_LEFT | wxALIGN_TOP);
    m_device_window->SetSizer(device_sizer);
    m_simple_book->AddPage(m_device_window, wxEmptyString, false);

    wxPanel *hor_line = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    hor_line->SetBackgroundColour("#ffffff");
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddSpacer(FromDIP(10));
    sizer->Add(hTopSizer, 0, wxEXPAND | wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, FromDIP(40));
    sizer->AddSpacer(FromDIP(10));
    sizer->Add(hor_line, 0, wxALIGN_CENTER_HORIZONTAL | wxEXPAND | wxLEFT | wxRIGHT, FromDIP(40));
    sizer->AddSpacer(FromDIP(40));
    sizer->Add(m_simple_book, 1, wxEXPAND | wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, FromDIP(40));
    sizer->AddSpacer(FromDIP(40));
    //sizer->Add(m_machinePanel, 1, wxEXPAND);

    this->SetSizer(sizer);
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
    m_placement_btn->Bind(wxEVT_LEFT_DOWN, &DeviceListPanel::onFilterButtonClicked, this);
    m_status_btn->Bind(wxEVT_LEFT_DOWN, &DeviceListPanel::onFilterButtonClicked, this);
    m_type_btn->Bind(wxEVT_LEFT_DOWN, &DeviceListPanel::onFilterButtonClicked, this);
    m_wlan_btn->Bind(wxEVT_TOGGLEBUTTON, &DeviceListPanel::onNetworkTypeToggled, this);
    m_lan_btn->Bind(wxEVT_TOGGLEBUTTON, &DeviceListPanel::onNetworkTypeToggled, this);
    m_static_btn->Bind(wxEVT_TOGGLEBUTTON, &DeviceListPanel::on_static_mode_toggled, this);
    MultiComMgr::inst()->Bind(COM_DEV_DETAIL_UPDATE_EVENT, &DeviceListPanel::onComDevDetailUpdate, this);
    wxGetApp().getDeviceObjectOpr()->Bind(EVT_DEVICE_LIST_UPDATED, &DeviceListPanel::onDeviceListUpdated, this);
    m_filter_popup->Bind(wxEVT_SHOW, &DeviceListPanel::onPopupShow, this);
}

void DeviceListPanel::initLocalDevice(std::map<std::string, DeviceItemPanel::DeviceInfo>& deviceInfoMap)
{
    deviceInfoMap.clear();
    AppConfig *config = wxGetApp().app_config;
    if (config) {
        std::vector<MacInfoMap> macInfo;
        config->get_local_mahcines(macInfo);
        DeviceItemPanel::DeviceInfo dev_info;
        dev_info.lanFlag = true;
        dev_info.status = "offline";
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
        //m_device_sizer->Add(item);
    }
    //m_device_window->Layout();
    //Layout();
    if (!m_device_map.empty()) {
        updateFilterMap();
    }
    filterDeviceList();
}

void DeviceListPanel::filterDeviceList()
{
    Freeze();
    m_device_sizer->Clear();
    for (const auto& iter : m_device_map) {
        const auto& dev_info = iter.second->deviceInfo();
        std::string type_str = FFUtils::getPrinterName(dev_info.pid);
        if ((m_filter_placement_default || dev_info.placement == m_filter_placement)
            && (m_filter_status_default || dev_info.status == m_filter_status)
            && (m_filter_types.find(type_str) != m_filter_types.end())
            && ((m_wlan_btn->GetValue() && !dev_info.lanFlag) || (m_lan_btn->GetValue() && dev_info.lanFlag))) {
            iter.second->Show(true);
            m_device_sizer->Add(iter.second);
        } else {
            iter.second->Show(false);
        }
    }
    m_device_window->Layout();
    Layout();
    Thaw();
}

void DeviceListPanel::updateFilterMap()
{
    updatePlacementMap();
    updateStatusMap();
    updateTypeMap();
}

void DeviceListPanel::updatePlacementMap()
{
    FilterItemMap backMap;
    for (const auto& iter : m_placement_item_map) {
        backMap.emplace(std::make_pair(iter.first, iter.second));
    }
    m_placement_item_map.clear();
    for (auto& iter : m_device_map) {
        std::string placement = iter.second->deviceInfo().placement;
        auto it = backMap.find(placement);
        if (it != backMap.end()) {
            m_placement_item_map.emplace(std::make_pair(placement, it->second));
            it->second = nullptr;
        } else if (m_placement_item_map.find(placement) == m_placement_item_map.end()) {
            auto item = new FilterPopupWindow::FilterItem(m_filter_popup, placement);
            item->Show(false);
            item->Bind(EVT_FILTER_ITEM_CLICKED, &DeviceListPanel::onFilterItemClicked, this);
            m_placement_item_map.emplace(std::make_pair(placement, item));
        }
    }
    for (auto& iter : backMap) {
        if (iter.second) {
            iter.second->Destroy();
            iter.second = nullptr;
        }
    }
    backMap.clear();
}

void DeviceListPanel::updateStatusMap()
{
    FilterItemMap backMap;
    for (const auto& iter : m_status_item_map) {
        backMap.emplace(std::make_pair(iter.first, iter.second));
    }
    m_status_item_map.clear();
    for (auto& iter : m_device_map) {
        std::string status = iter.second->deviceInfo().status;
        auto it = backMap.find(status);
        if (it != backMap.end()) {
            m_status_item_map.emplace(std::make_pair(status, it->second));
            it->second = nullptr;
        } else if (m_status_item_map.find(status) == m_status_item_map.end()) {
            auto item = new FilterPopupWindow::FilterItem(m_filter_popup, status);
            item->Bind(EVT_FILTER_ITEM_CLICKED, &DeviceListPanel::onFilterItemClicked, this);
            item->Show(false);
            m_status_item_map.emplace(std::make_pair(status, item));
        }
    }
    for (auto& iter : backMap) {
        if (iter.second) {
            iter.second->Destroy();
            iter.second = nullptr;
        }
    }
    backMap.clear();
}

void DeviceListPanel::updateTypeMap()
{
    FilterItemMap backMap;
    for (const auto& iter : m_type_item_map) {
        backMap.emplace(std::make_pair(iter.first, iter.second));
    }
    m_type_item_map.clear();
    m_filter_types.clear();
    for (auto& iter : m_device_map) {
        auto pid = iter.second->deviceInfo().pid;
        auto type_str = FFUtils::getPrinterName(pid);
        auto it = backMap.find(type_str);
        if (it != backMap.end()) {
            m_type_item_map.emplace(std::make_pair(type_str, it->second));
            it->second = nullptr;
        } else if (m_type_item_map.find(type_str) == m_type_item_map.end()) {
            auto item = new FilterPopupWindow::FilterItem(m_filter_popup, type_str, false, false, true);
            item->Bind(EVT_FILTER_ITEM_CLICKED, &DeviceListPanel::onFilterItemClicked, this);
            bool show = item->Show(false);
            m_type_item_map.emplace(std::make_pair(type_str, item));
        }
        if (m_filter_types.find(type_str) == m_filter_types.end()) {
            m_filter_types.emplace(type_str);
        }
    }
    for (auto& iter : backMap) {
        if (iter.second) {
            iter.second->Destroy();
            iter.second = nullptr;
        }
    }
    backMap.clear();
}

void DeviceListPanel::onPopupShow(wxShowEvent& event)
{
    bool show = event.IsShown();
    //BOOST_LOG_TRIVIAL(error) << "onPopupShow: " << show ? "true" : "false";
    //flush_logs();
    for (auto& iter : m_device_map)  {
        if (iter.second) {
            iter.second->blockMouseEvent(show);
        }
    }
}

void DeviceListPanel::onFilterItemClicked(wxCommandEvent& event)
{
    if (Filter_Popup_Type_Placement == m_filter_popup_type) {
        if (event.GetEventObject() == m_default_filter_item) {
            m_filter_placement_default = true;
            m_filter_placement = "";
        } else {
            m_filter_placement_default = false;
            m_filter_placement = event.GetString().ToStdString();
        }
        m_filter_popup->Dismiss();
    } else if (Filter_Popup_Type_Status == m_filter_popup_type) {
        if (event.GetEventObject() == m_default_filter_item) {
            m_filter_status_default = true;
            m_filter_status = "";
        } else {
            m_filter_status_default = false;
            m_filter_status = event.GetString().ToStdString();
        }
        m_filter_popup->Dismiss();
    } else if (Filter_Popup_Type_Device_Type == m_filter_popup_type) {
        auto type_str = event.GetString().ToStdString();
        auto iter = m_filter_types.find(type_str);
        if (iter == m_filter_types.end()) {
            m_filter_types.emplace(type_str);
        } else {
            m_filter_types.erase(iter);
        }
    }
    filterDeviceList();
}

void DeviceListPanel::onFilterButtonClicked(wxMouseEvent &event)
{
    wxPoint pos;
    m_filter_popup->ClearItem();
    if (event.GetEventObject() == m_placement_btn) {
        m_filter_popup_type = Filter_Popup_Type_Placement;
        m_default_filter_item->setBottomCornerRound(m_placement_item_map.empty());
        m_default_filter_item->setSelect(m_filter_placement_default);
        m_filter_popup->AddItem(m_default_filter_item);
        for (auto& iter : m_placement_item_map) {
            iter.second->setBottomCornerRound(false);
            iter.second->setSelect(!m_filter_placement_default && (iter.first == m_filter_placement));
            m_filter_popup->AddItem(iter.second);
        }
        if (!m_placement_item_map.empty()) {
            m_placement_item_map.rbegin()->second->setBottomCornerRound(true);
        }
        pos = m_placement_btn->ClientToScreen(wxPoint(0, 0));
        pos.y += m_placement_btn->GetSize().y + 2;
    } else if (event.GetEventObject() == m_status_btn) {
        m_filter_popup_type = Filter_Popup_Type_Status;
        m_default_filter_item->setBottomCornerRound(m_status_item_map.empty());
        m_default_filter_item->setSelect(m_filter_status_default);
        m_filter_popup->AddItem(m_default_filter_item);
        for (auto& iter : m_status_item_map) {
            iter.second->setBottomCornerRound(false);
            iter.second->setSelect(!m_filter_status_default && (iter.first == m_filter_status));
            m_filter_popup->AddItem(iter.second);
        }
        if (!m_status_item_map.empty()) {
            m_status_item_map.rbegin()->second->setBottomCornerRound(true);
        }
        pos = m_status_btn->ClientToScreen(wxPoint(0, 0));
        pos.y += m_status_btn->GetSize().y + 2;
    } else if (event.GetEventObject() == m_type_btn) {
        m_filter_popup_type = Filter_Popup_Type_Device_Type;
        for (auto& iter : m_type_item_map) {
            iter.second->setBottomCornerRound(false);
            iter.second->setChecked(m_filter_types.find(iter.first) != m_filter_types.end());
            m_filter_popup->AddItem(iter.second);
        }
        if (!m_type_item_map.empty()) {
            m_type_item_map.begin()->second->setTopCornerRound(true);
            m_type_item_map.rbegin()->second->setBottomCornerRound(true);
        }
        pos = m_type_btn->ClientToScreen(wxPoint(0, 0));
        pos.y += m_type_btn->GetSize().y + 2;
    }
    m_filter_popup->Move(pos);
    m_filter_popup->Popup();
}

void DeviceListPanel::onNetworkTypeToggled(wxCommandEvent& event)
{
    filterDeviceList();
}

void DeviceListPanel::on_static_mode_toggled(wxCommandEvent &event)
{
    //m_simple_book->SetSelection(1);
    //m_simple_book->Layout();
    //Layout();
    event.Skip();
}

void DeviceListPanel::onDeviceListUpdated(DeviceListUpdateEvent& event)
{
    std::string dev_id = event.GetDeviceId();
    int conn_id = event.GetConnectionId();
    bool valid = false;
    const com_dev_data_t &data = MultiComMgr::inst()->devData(conn_id, &valid);
    if (valid) {
        DeviceItemPanel::DeviceInfo dev_info;
        dev_info.conn_id = conn_id;
        dev_info.lanFlag = (COM_CONNECT_LAN == data.connectMode);
        dev_info.name = data.devDetail->name;
        dev_info.pid = data.devDetail->pid;
        dev_info.placement = data.devDetail->location;
        dev_info.status = data.devDetail->status;
        if (dev_info.status.empty()) dev_info.status = "offline";

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
    updateFilterMap();
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