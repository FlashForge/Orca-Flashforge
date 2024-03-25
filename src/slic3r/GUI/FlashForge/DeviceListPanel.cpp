#include "DeviceListPanel.hpp"
#include <boost/log/trivial.hpp>
#include <wx/graphics.h>
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/FFUtils.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/Widgets/FFToggleButton.hpp"
#include "slic3r/GUI/Widgets/FFCheckBox.hpp"
#include "slic3r/GUI/wxExtensions.hpp"
//#include "DeviceData.hpp"
#include "slic3r/GUI/Monitor.hpp"
#include "slic3r/GUI/MainFrame.hpp"

namespace Slic3r {
namespace GUI {

DropDownButton::DropDownButton(wxWindow* parent/*=nullptr*/, const wxString& text/*=wxEmptyString*/, const wxBitmap& bitmap/*=wxNullBitmap*/)
    : wxPanel(parent)
{
    //SetMinSize(wxSize(FromDIP(80), FromDIP(24)));
    if (parent) {
        SetBackgroundColour(parent->GetBackgroundColour());
    }
    m_text = new wxStaticText(this, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    //m_text->FitInside();
    m_text->Fit();
    m_bitmap = new wxStaticBitmap(this, wxID_ANY, bitmap, wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    if (!bitmap.IsNull()) {
        auto sz = bitmap.GetSize();
        m_bitmap->SetSize(bitmap.GetSize());
        m_bitmap->SetMinSize(bitmap.GetSize());
    }
    m_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_sizer->Add(m_text, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, FromDIP(3));
    m_sizer->AddSpacer(FromDIP(12));
    m_sizer->Add(m_bitmap, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, FromDIP(3));
    m_sizer->AddStretchSpacer(1);

    updateMinSize();
    SetSizer(m_sizer);
    Layout();
    Fit();
    //initControl();
    //setCustomBoxSizer();

    Bind(wxEVT_ENTER_WINDOW, &DropDownButton::onEnter, this);
	Bind(wxEVT_LEAVE_WINDOW, &DropDownButton::onLeave, this);
    Bind(wxEVT_MOTION, &DropDownButton::onMotion, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &DropDownButton::onMouseCaptureLost, this);
    m_text->Bind(wxEVT_ENTER_WINDOW, &DropDownButton::onEnter, this);
    m_bitmap->Bind(wxEVT_ENTER_WINDOW, &DropDownButton::onEnter, this);
}

DropDownButton::~DropDownButton()
{
    leaveWindow();
}

void DropDownButton::setText(const wxString& text)
{
    m_text->SetLabel(text);
    updateMinSize();
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

void DropDownButton::leaveWindow()
{
    if (HasCapture()) {
        ReleaseMouse();
        //BOOST_LOG_TRIVIAL(error) << "DropDownButton ReleaseMouse";
        //flush_logs();
    }
}

void DropDownButton::updateMinSize()
{
    wxScreenDC dc;
    dc.SetFont(GetFont());
    auto text_size = dc.GetTextExtent(m_text->GetLabel());
    int width = text_size.x + 2 * FromDIP(12) + m_bitmap->GetSize().x;
    SetMinSize(wxSize(width, FromDIP(24)));
    Layout();
    Fit();
}

void DropDownButton::onEnter(wxMouseEvent& event)
{
    if (isPointIn(convertEventPoint(event))) {
        if (!HasCapture()) {
            CaptureMouse();
            //BOOST_LOG_TRIVIAL(error) << "DropDownButton CaptureMouse";
            //flush_logs();
        }
    }
    event.Skip();
}

void DropDownButton::onLeave(wxMouseEvent& event)
{
    leaveWindow();
    event.Skip();
}

void DropDownButton::onMotion(wxMouseEvent& event)
{
    if (!isPointIn(event.GetPosition())) {
        leaveWindow();
    }
    event.Skip();
}

void DropDownButton::onMouseCaptureLost(wxMouseCaptureLostEvent& event)
{
    leaveWindow();
    event.Skip();
}

bool DropDownButton::isPointIn(const wxPoint& pnt)
{
    return !(wxHT_WINDOW_OUTSIDE == HitTest(pnt));
}


wxDEFINE_EVENT(EVT_FILTER_ITEM_CLICKED, wxCommandEvent);
FilterPopupWindow::FilterItem::FilterItem(wxWindow* parent, const wxString& text, bool top_corner_round/*=false*/, bool bottom_corner_round/*=false*/)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER)
    , m_topCornerRound(top_corner_round)
    , m_bottomCornerRound(bottom_corner_round)
{
    //SetBackgroundColour(wxColour("#eeeeee"));
    SetMinSize(wxSize(FromDIP(80), FromDIP(30)));
    SetMaxSize(wxSize(-1, FromDIP(30)));
    SetSize(wxSize(-1, FromDIP(30)));

    m_main_sizer = new wxBoxSizer(wxHORIZONTAL);
    SetSizer(m_main_sizer);

    m_text = new wxStaticText(this, wxID_ANY, text);
    m_main_sizer->AddSpacer(FromDIP(15));
    m_main_sizer->Add(m_text, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, FromDIP(5));
    m_main_sizer->AddSpacer(FromDIP(15));
    Layout();
    Fit();

    Bind(wxEVT_PAINT, &FilterItem::onPaint, this);
    Bind(wxEVT_ENTER_WINDOW, &FilterItem::onEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &FilterItem::onLeave, this);
    Bind(wxEVT_LEFT_DOWN, &FilterItem::onMouseDown, this);
    Bind(wxEVT_LEFT_UP, &FilterItem::onMouseUp, this);
    Bind(wxEVT_MOTION, &FilterItem::onMotion, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &FilterItem::onMouseCaptureLost, this);
    m_text->Bind(wxEVT_ENTER_WINDOW, &FilterItem::onEnter, this);
}

FilterPopupWindow::FilterItem::~FilterItem()
{
    leaveWindow();
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
    updateChildrenBackground(color);
    //m_text->SetBackgroundColour(color);
    //m_check_box->SetBackgroundColour(color);
}

void FilterPopupWindow::FilterItem::updateChildrenBackground(const wxColour& color)
{
    m_text->SetBackgroundColour(color);
}

wxPoint FilterPopupWindow::FilterItem::convertEventPoint(wxMouseEvent& event)
{
    wxPoint pnt = event.GetPosition();
    if (event.GetId() == m_text->GetId()) {
        pnt += m_text->GetPosition();
    }
    return pnt;
}

void FilterPopupWindow::FilterItem::onEnter(wxMouseEvent& event)
{
    if (isPointIn(convertEventPoint(event))) {
        if (!HasCapture()) {
            //BOOST_LOG_TRIVIAL(error) << "FilterItem CaptureMouse";
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
    //BOOST_LOG_TRIVIAL(error) << "FilterItem::onLeave";
    //flush_logs();
    leaveWindow();
    event.Skip();
}

void FilterPopupWindow::FilterItem::onMouseDown(wxMouseEvent& event)
{
    if (isPointIn(event.GetPosition())) {
        mouseDownEvent();
        m_pressFlag = true;
        Refresh();
    }
    event.Skip();
}

void FilterPopupWindow::FilterItem::onMouseUp(wxMouseEvent& event)
{
    if (isPointIn(event.GetPosition())) {
        m_pressFlag = false;
        mouseUpEvent();
        Refresh();
    }
    event.Skip();
}

void FilterPopupWindow::FilterItem::onMotion(wxMouseEvent& event)
{
    if (!isPointIn(event.GetPosition())) {
        leaveWindow();
    }
    event.Skip();
}

void FilterPopupWindow::FilterItem::onMouseCaptureLost(wxMouseCaptureLostEvent& event)
{
    leaveWindow();
    event.Skip();
}

void FilterPopupWindow::FilterItem::leaveWindow()
{
    if (HasCapture()) {
        ReleaseMouse();
        //BOOST_LOG_TRIVIAL(error) << "FilterItem ReleaseMouse";
        //flush_logs();
    }
    m_hoverFlag = false;
    m_pressFlag = false;
    Refresh();
}

void FilterPopupWindow::FilterItem::sendEvent(const wxString& str_data, int int_data)
{
    wxCommandEvent event(EVT_FILTER_ITEM_CLICKED);
    event.SetEventObject(this);
    event.SetString(str_data);
    event.SetInt(int_data);
    wxPostEvent(this, event);
}

bool FilterPopupWindow::FilterItem::isPointIn(const wxPoint& pnt)
{
    return !(wxHT_WINDOW_OUTSIDE == HitTest(pnt));
}

void FilterPopupWindow::FilterItem::mouseUpEvent()
{
    sendEvent(m_text->GetLabel(), 0);
}


FilterPopupWindow::StatusItem::StatusItem(wxWindow* parent, const std::string& status,
    bool top_corner_round/* = false*/, bool bottom_corner_round/* = false*/)
    : FilterItem(parent, wxEmptyString, top_corner_round, bottom_corner_round)
    , m_status(status)
{
    setStatus(m_status);
}


void FilterPopupWindow::StatusItem::mouseUpEvent()
{
    sendEvent(m_status, 0);
}

void FilterPopupWindow::StatusItem::setStatus(const std::string& status)
{
    wxColour color;
    m_text->SetLabel(FFUtils::convertStatus(m_status, color));
    Layout();
    Fit();
}


FilterPopupWindow::DeviceTypeItem::DeviceTypeItem(wxWindow* parent, unsigned short pid,
    bool checked/* = false*/, bool top_corner_round/* = false*/, bool bottom_corner_round/* = false*/)
    : FilterItem(parent, wxEmptyString, top_corner_round, bottom_corner_round)
    , m_pid(pid)
{
    m_main_sizer->Clear();
    m_check_box = new FFCheckBox(this);
    m_check_box->SetValue(checked);
    m_text->SetLabel(FFUtils::getPrinterName(m_pid));
    m_main_sizer->AddSpacer(FromDIP(15));
    m_main_sizer->Add(m_check_box, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxTOP | wxBOTTOM, FromDIP(5));
    m_main_sizer->Add(m_text, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, FromDIP(5));
    m_main_sizer->AddSpacer(FromDIP(15));
    Layout();
    Fit();
    m_check_box->Bind(wxEVT_ENTER_WINDOW, &DeviceTypeItem::onEnter, this);
}

bool FilterPopupWindow::DeviceTypeItem::isChecked() const
{
    return m_check_box->GetValue();
}

void FilterPopupWindow::DeviceTypeItem::setChecked(bool checked)
{
    m_check_box->SetValue(checked);
}

wxPoint FilterPopupWindow::DeviceTypeItem::convertEventPoint(wxMouseEvent& event)
{
    if (event.GetId() == m_check_box->GetId()) {
        return event.GetPosition() + m_check_box->GetPosition();
    } 
    return FilterItem::convertEventPoint(event);
}

void FilterPopupWindow::DeviceTypeItem::updateChildrenBackground(const wxColour& color)
{
    m_check_box->SetBackgroundColour(color);
    FilterItem::updateChildrenBackground(color);
}

void FilterPopupWindow::DeviceTypeItem::mouseDownEvent()
{
    m_check_box->SetValue(!m_check_box->GetValue());
    sendEvent("", m_pid);
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

void FilterPopupWindow::OnDismiss()
{
    ClearItem();
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


//wxDEFINE_EVENT(EVT_DEVICE_ITEM_SELECTED, wxCommandEvent);
std::map<unsigned short, wxBitmap> DeviceItemPanel::m_machineBitmapMap;
DeviceItemPanel::DeviceItemPanel(wxWindow *parent) 
    : wxPanel(parent)
{
    SetSize(FromDIP(220), FromDIP(205));
    SetMinSize(wxSize(FromDIP(220), FromDIP(205)));
    SetMaxSize(wxSize(FromDIP(220), FromDIP(205)));

    m_main_sizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(m_main_sizer);
    Layout();

    Bind(wxEVT_LEFT_DOWN, &DeviceItemPanel::mouseDown, this);
    Bind(wxEVT_LEFT_UP, &DeviceItemPanel::mouseReleased, this);
    Bind(wxEVT_ENTER_WINDOW, &DeviceItemPanel::onEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &DeviceItemPanel::onLeave, this);
    Bind(wxEVT_MOTION, &DeviceItemPanel::onMotion, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &DeviceItemPanel::onMouseCaptionLost, this);
    Bind(wxEVT_PAINT, &DeviceItemPanel::onPaint, this);
}

DeviceItemPanel::~DeviceItemPanel()
{
    leaveWindow();
}

void DeviceItemPanel::blockMouseEvent(bool block)
{
    if (block) {
        Unbind(wxEVT_LEFT_DOWN, &DeviceItemPanel::mouseDown, this);
        Unbind(wxEVT_LEFT_UP, &DeviceItemPanel::mouseReleased, this);
        Unbind(wxEVT_ENTER_WINDOW, &DeviceItemPanel::onEnter, this);
        Unbind(wxEVT_LEAVE_WINDOW, &DeviceItemPanel::onLeave, this);
        Unbind(wxEVT_MOTION, &DeviceItemPanel::onMotion, this);
    } else {
        Bind(wxEVT_LEFT_DOWN, &DeviceItemPanel::mouseDown, this);
        Bind(wxEVT_LEFT_UP, &DeviceItemPanel::mouseReleased, this);
        Bind(wxEVT_ENTER_WINDOW, &DeviceItemPanel::onEnter, this);
        Bind(wxEVT_LEAVE_WINDOW, &DeviceItemPanel::onLeave, this);
        Bind(wxEVT_MOTION, &DeviceItemPanel::onMotion, this);
    }
}

wxPoint DeviceItemPanel::getEventPoint(wxMouseEvent& event)
{
    return event.GetPosition();
}

bool DeviceItemPanel::isPointIn(const wxPoint& pt)
{
    return !(wxHT_WINDOW_OUTSIDE == HitTest(pt));
}

void DeviceItemPanel::enterWindow()
{
    m_hovered = true;
    Refresh();
    if (!HasCapture()) {
        CaptureMouse();
        //BOOST_LOG_TRIVIAL(error) << "DeviceItemPanel CaptureMouse";
        //flush_logs();
    }
}

void DeviceItemPanel::leaveWindow()
{
    m_hovered = false;
    m_pressed = false;
    Refresh();
    if (HasCapture()) {
        ReleaseMouse();
        //BOOST_LOG_TRIVIAL(error) << "DeviceItemPanel ReleaseMouse";
        //flush_logs();
    }
}

void DeviceItemPanel::mouseDown(wxMouseEvent& event)
{
    if (isPointIn(getEventPoint(event))) {
        m_pressed = true;
        Refresh();
    }
    event.Skip();
}

void DeviceItemPanel::mouseReleased(wxMouseEvent& event)
{
    if (isPointIn(getEventPoint(event))) {
        sendEvent();
    }
    m_pressed = false;
    Refresh();
    event.Skip();
}

void DeviceItemPanel::onEnter(wxMouseEvent& event)
{
    if (isPointIn(getEventPoint(event))) {
        enterWindow();
    }
    event.Skip();
}

void DeviceItemPanel::onLeave(wxMouseEvent& event)
{
    //BOOST_LOG_TRIVIAL(error) << "DeviceItemPanel::onLeave";
    //flush_logs();
    leaveWindow();
    event.Skip();
}

void DeviceItemPanel::onMotion(wxMouseEvent& event)
{
    if (isPointIn(getEventPoint(event))) {
        enterWindow();
    } else {
        leaveWindow();
    }
    event.Skip();
}

void DeviceItemPanel::onMouseCaptionLost(wxMouseCaptureLostEvent& event)
{
    leaveWindow();
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


DeviceInfoItemPanel::DeviceInfoItemPanel(wxWindow *parent, const DeviceInfo& info, wxWindow* event_handle/*=nullptr*/)
    : DeviceItemPanel(parent)
    , m_event_handle(event_handle)
{
    Freeze();
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

    m_main_sizer->AddStretchSpacer(1);
    m_main_sizer->Add(m_name_text, 0, wxEXPAND | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(10));
    m_main_sizer->AddSpacer(FromDIP(3));
    m_main_sizer->Add(m_icon, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, FromDIP(10));
    m_main_sizer->AddSpacer(FromDIP(3));
    m_main_sizer->Add(m_placement_text, 0, wxEXPAND | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(10));
    m_main_sizer->AddSpacer(FromDIP(3));
    m_main_sizer->Add(m_status_text, 0, wxEXPAND | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(10));
    m_main_sizer->AddStretchSpacer(1);
    
    updateInfo(info);
    Layout();
    Thaw();
    m_name_text->Bind(wxEVT_MOTION, &DeviceInfoItemPanel::onMotion, this);
    m_icon->Bind(wxEVT_MOTION, &DeviceInfoItemPanel::onMotion, this);
    m_placement_text->Bind(wxEVT_MOTION, &DeviceInfoItemPanel::onMotion, this);
    m_status_text->Bind(wxEVT_MOTION, &DeviceInfoItemPanel::onMotion, this);
}

void DeviceInfoItemPanel::updateInfo(const DeviceInfo& info)
{
    m_name_text->SetLabel(wxString::FromUTF8(info.name));
    //if (info.placement.empty()) {
    //    m_placement_text->SetLabel(_L("Default"));
    //} else {
    m_placement_text->SetLabel(wxString::FromUTF8(info.placement));
    //}
    if (info.pid != m_info.pid) {
        m_icon->SetBitmap(machineBitmap(info.pid));
    }
    m_info = info;
    updateStatus();
    Layout();
}

const DeviceInfoItemPanel::DeviceInfo& DeviceInfoItemPanel::deviceInfo() const
{
    return m_info;
}

void DeviceInfoItemPanel::blockMouseEvent(bool block)
{
    if (block) {
        m_name_text->Unbind(wxEVT_MOTION, &DeviceInfoItemPanel::onMotion, this);
        m_icon->Unbind(wxEVT_MOTION, &DeviceInfoItemPanel::onMotion, this);
        m_placement_text->Unbind(wxEVT_MOTION, &DeviceInfoItemPanel::onMotion, this);
        m_status_text->Unbind(wxEVT_MOTION, &DeviceInfoItemPanel::onMotion, this);
    } else {
        m_name_text->Bind(wxEVT_MOTION, &DeviceInfoItemPanel::onMotion, this);
        m_icon->Bind(wxEVT_MOTION, &DeviceInfoItemPanel::onMotion, this);
        m_placement_text->Bind(wxEVT_MOTION, &DeviceInfoItemPanel::onMotion, this);
        m_status_text->Bind(wxEVT_MOTION, &DeviceInfoItemPanel::onMotion, this);
    }
    DeviceItemPanel::blockMouseEvent(block);
}

wxPoint DeviceInfoItemPanel::getEventPoint(wxMouseEvent& event)
{
    wxPoint pnt = event.GetPosition();
    if (event.GetEventObject() == m_name_text) {
        pnt += m_name_text->GetPosition();
    } else if (event.GetEventObject() == m_icon) {
        pnt += m_icon->GetPosition();
    } else if (event.GetEventObject() == m_placement_text) {
        pnt += m_placement_text->GetPosition();
    } else if (event.GetEventObject() == m_status_text) {
        pnt += m_status_text->GetPosition();
    }
    return pnt;
}

void DeviceInfoItemPanel::sendEvent()
{
    if (m_info.conn_id >= 0 && !m_info.status.empty() && m_info.status != "offline" && m_event_handle) {
        wxGetApp().mainframe->jump_to_monitor(EVT_SWITCH_TO_DEVICE_STATUS, m_info.conn_id);
        //wxCommandEvent event(EVT_DEVICE_ITEM_SELECTED, GetId());
        //event.SetEventObject(m_event_handle);
        //event.SetInt(m_info.conn_id);
        //wxPostEvent(m_event_handle, event);
    }
}

void DeviceInfoItemPanel::updateStatus()
{
    if (m_info.conn_id < 0) {
        m_info.status = "offline";
    }
    wxColour color("#00CD6D");
    wxString status = FFUtils::convertStatus(m_info.status, color);
    m_status_text->SetLabel(status);
    m_status_text->SetForegroundColour(color);
}

wxBitmap DeviceInfoItemPanel::machineBitmap(unsigned short pid)
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


DeviceStaticItemPanel::DeviceStaticItemPanel(wxWindow* parent, std::string status, int count)
    : DeviceItemPanel(parent)
    , m_count(count)
    , m_status(status)
{
    Freeze();
    wxColour color;
    wxString wxstatus = FFUtils::convertStatus(m_status, color);
    m_status_text = new wxStaticText(this, wxID_ANY, wxstatus, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    m_status_text->SetForegroundColour(color);
    m_status_text->SetBackgroundColour(wxColour("#ffffff"));
    m_count_text = new wxStaticText(this, wxID_ANY, wxString::Format("%d", m_count), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    m_count_text->SetFont(::Label::Head_18);
    m_count_text->SetBackgroundColour(wxColour("#ffffff"));

    m_main_sizer->AddStretchSpacer(1);
    m_main_sizer->Add(m_status_text, 0, wxEXPAND | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(10));
    m_main_sizer->AddSpacer(FromDIP(40));
    m_main_sizer->Add(m_count_text, 0, wxEXPAND | wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, FromDIP(10));
    m_main_sizer->AddSpacer(FromDIP(50));
    m_main_sizer->AddStretchSpacer(1);
    Layout();
    Thaw();
    m_status_text->Bind(wxEVT_MOTION, &DeviceStaticItemPanel::onMotion, this);
    m_count_text->Bind(wxEVT_MOTION, &DeviceStaticItemPanel::onMotion, this);
}

void DeviceStaticItemPanel::setStatus(const std::string& status)
{
    wxColour color;
    wxString wxstatus = FFUtils::convertStatus(m_status, color);
    m_status_text->SetLabel(wxstatus);
    m_status_text->SetBackgroundColour(color);
    Layout();
}
void DeviceStaticItemPanel::setCount(int count)
{
    m_count = count;
    m_count_text->SetLabel(wxString::Format("%d", m_count));
    Layout();
}

void DeviceStaticItemPanel::blockMouseEvent(bool block)
{
    if (block) {
        m_status_text->Unbind(wxEVT_MOTION, &DeviceStaticItemPanel::onMotion, this);
        m_count_text->Unbind(wxEVT_MOTION, &DeviceStaticItemPanel::onMotion, this);
    } else {
        m_status_text->Bind(wxEVT_MOTION, &DeviceStaticItemPanel::onMotion, this);
        m_count_text->Bind(wxEVT_MOTION, &DeviceStaticItemPanel::onMotion, this);
    }
    DeviceItemPanel::blockMouseEvent(block);
}

wxPoint DeviceStaticItemPanel::getEventPoint(wxMouseEvent& event)
{
    wxPoint pnt = event.GetPosition();
    if (event.GetEventObject() == m_status_text) {
        pnt += m_status_text->GetPosition();
    } else if (event.GetEventObject() == m_count_text) {
        pnt += m_count_text->GetPosition();
    }
    return pnt;
}


int DeviceListPanel::m_last_priority_id = 0;
DeviceListPanel::DeviceListPanel(wxWindow* parent, wxWindowID id, const wxPoint& pos, 
        const wxSize& size)
    : wxPanel(parent, id, pos, size, wxTAB_TRAVERSAL)
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
    //SetScrollRate(5, 5);
    m_filter_popup = new FilterPopupWindow(this);
    m_default_filter_item = new FilterPopupWindow::FilterItem(m_filter_popup, _L("All"), true);
    m_default_filter_item->Bind(EVT_FILTER_ITEM_CLICKED, &DeviceListPanel::onFilterItemClicked, this);
    m_default_filter_item->Show(false);
    SetBackgroundColour(wxColour("#F0F0F0"));
    m_placement_btn = new DropDownButton(this, _L("All"), create_scaled_bitmap("device_dropdown", this, 8));
    m_status_btn  = new DropDownButton(this, _L("Status"), create_scaled_bitmap("device_dropdown", this, 8));
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
    hTopSizer->Add(m_placement_btn, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT);
    hTopSizer->AddSpacer(FromDIP(50));
    hTopSizer->Add(m_status_btn, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT);
    hTopSizer->AddSpacer(FromDIP(50));
    hTopSizer->Add(m_type_btn, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT);
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

    m_device_scrolled_window = new wxScrolledWindow(m_simple_book, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL | wxVSCROLL);
    m_device_scrolled_window->SetScrollRate(50, 50);
    m_device_panel = new wxPanel(m_device_scrolled_window);
    wxBoxSizer* scroll_sizer = new wxBoxSizer(wxVERTICAL);
    scroll_sizer->AddSpacer(FromDIP(30));
    scroll_sizer->Add(m_device_panel, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    scroll_sizer->AddSpacer(FromDIP(30));
    m_device_scrolled_window->SetSizer(scroll_sizer);
    //m_device_window->EnableScrolling(true, true);
    //m_device_window->SetScrollRate(0, 10);
    wxBoxSizer* device_sizer = new wxBoxSizer(wxVERTICAL);
    //m_device_panel->SetBackgroundColour(wxColour("#F0F0F0"));
    m_device_sizer = new wxGridSizer(5);
    m_device_sizer->SetHGap(FromDIP(30));
    m_device_sizer->SetVGap(FromDIP(30));
    device_sizer->Add(m_device_sizer, 0, wxALIGN_LEFT | wxALIGN_TOP);
    m_device_panel->SetSizer(device_sizer);
    m_simple_book->AddPage(m_device_scrolled_window, wxEmptyString, false);

    wxPanel *hor_line = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    hor_line->SetBackgroundColour("#ffffff");
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddSpacer(FromDIP(10));
    sizer->Add(hTopSizer, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(30));
    sizer->AddSpacer(FromDIP(10));
    sizer->Add(hor_line, 0, wxALIGN_CENTER_HORIZONTAL | wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    //sizer->AddSpacer(FromDIP(30));
    sizer->Add(m_simple_book, 1, wxEXPAND | wxALIGN_CENTER_HORIZONTAL);
    //sizer->AddSpacer(FromDIP(30));
    //sizer->Add(m_machinePanel, 1, wxEXPAND);

    this->SetSizer(sizer);
    this->Layout();
    this->Fit();
}

void DeviceListPanel::OnActivate()
{
    updateDeviceList();
}

void DeviceListPanel::connectEvent()
{
    m_placement_btn->Bind(wxEVT_LEFT_DOWN, &DeviceListPanel::onFilterButtonClicked, this);
    m_status_btn->Bind(wxEVT_LEFT_DOWN, &DeviceListPanel::onFilterButtonClicked, this);
    m_type_btn->Bind(wxEVT_LEFT_DOWN, &DeviceListPanel::onFilterButtonClicked, this);
    m_wlan_btn->Bind(wxEVT_TOGGLEBUTTON, &DeviceListPanel::onNetworkTypeToggled, this);
    m_lan_btn->Bind(wxEVT_TOGGLEBUTTON, &DeviceListPanel::onNetworkTypeToggled, this);
    m_static_btn->Bind(wxEVT_TOGGLEBUTTON, &DeviceListPanel::onStaticModeToggled, this);
    MultiComMgr::inst()->Bind(COM_DEV_DETAIL_UPDATE_EVENT, &DeviceListPanel::onComDevDetailUpdate, this);
    MultiComMgr::inst()->Bind(COM_WAN_DEV_INFO_UPDATE_EVENT, &DeviceListPanel::onComWanDeviceInfoUpdate, this);
    wxGetApp().getDeviceObjectOpr()->Bind(EVT_DEVICE_LIST_UPDATED, &DeviceListPanel::onDeviceListUpdated, this);
    m_filter_popup->Bind(wxEVT_SHOW, &DeviceListPanel::onPopupShow, this);
    m_refresh_timer.Bind(wxEVT_TIMER, &DeviceListPanel::onRefreshTimeout, this);
}

void DeviceListPanel::initLocalDevice(std::map<std::string, DeviceInfoItemPanel::DeviceInfo>& deviceInfoMap)
{
    deviceInfoMap.clear();
    AppConfig *config = wxGetApp().app_config;
    if (config) {
        std::vector<MacInfoMap> macInfo;
        config->get_local_mahcines(macInfo);
        DeviceInfoItemPanel::DeviceInfo dev_info;
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

void DeviceListPanel::initWlanDevice(std::map<std::string, DeviceInfoItemPanel::DeviceInfo>& deviceInfoMap)
{
    com_id_list_t idList = MultiComMgr::inst()->getReadyDevList();
    for (const auto& id : idList) {
        DeviceInfoItemPanel::DeviceInfo dev_info;
        if (getDeviceInfo(dev_info, id)) {
            bool valid = false;
            const auto& data = MultiComMgr::inst()->devData(id, &valid);
            std::string dev_id = (COM_CONNECT_LAN == data.connectMode) ? data.lanDevInfo.serialNumber : data.wanDevInfo.serialNumber;
            auto iter = deviceInfoMap.find(dev_id);
            if (iter == deviceInfoMap.end()) {
                deviceInfoMap.emplace(std::make_pair(dev_id, dev_info));
            } else {// if (dev_info.status != "offline" || dev_info.lanFlag) {
                iter->second = dev_info;
            }
        }
    }
}

void DeviceListPanel::initDeviceList()
{
    std::map<std::string, DeviceInfoItemPanel::DeviceInfo> devList;
    initLocalDevice(devList);
    initWlanDevice(devList);  

    if (devList.empty()) {
        m_simple_book->ChangeSelection(0);
        return;
    }
    m_simple_book->ChangeSelection(1);

    std::vector<std::string> devKeyList;
    for (const auto& it: devList) {
        devKeyList.emplace_back(it.first);
    }
    std::sort(devKeyList.begin(), devKeyList.end(), [&devList](auto& a, auto&b) {
        return devList[a].name.compare(devList[b].name) < 0;
    });
    //m_device_map
    for (const auto& iter : devKeyList) {
        DeviceKey key(m_last_priority_id, iter, devList[iter].name);
        DeviceInfoItemPanel* item = new DeviceInfoItemPanel(m_device_panel, devList[iter], this);
        item->Show(false);
        m_device_map.emplace(std::make_pair(key, item));
        //m_device_sizer->Add(item);
    }
    //std::sort(m_device_map.begin(), m_device_map.end(), deviceKeySortFunc);
    // m_device_stat_map
    std::map<std::string, int> statusMap;
    for (const auto& dev : devList) {
        auto iter = statusMap.find(dev.second.status);
        if (iter != statusMap.end()) {
            iter->second += 1;   
        } else {
            statusMap.emplace(std::make_pair(dev.second.status, 1));
        }
    }
    for (const auto& iter : statusMap) {
        auto item = new DeviceStaticItemPanel(m_device_panel, iter.first, iter.second);
        item->Show(false);
        m_device_stat_map.emplace(std::make_pair(iter.first, item));
    }

    //m_device_window->Layout();
    //Layout();
    if (!m_device_map.empty()) {
        updateFilterMap();
    }
    filterDeviceList();
    updateDeviceWindowSize();
}

void DeviceListPanel::filterDeviceList()
{
    if (m_static_btn->GetValue()) {
        return;
    }
    Freeze();
    for (const auto& iter : m_device_stat_map) {
        iter.second->Show(false);
    }
    DeviceKeySet device_key_set;
    for (const auto& iter : m_device_map) {
        device_key_set.emplace(iter.first);
    }

    m_device_sizer->Clear();
    for (const auto& key : device_key_set) {
        const auto& iter = m_device_map.find(key);
        if (iter == m_device_map.end()) {
            continue;
        }
        const auto& dev_info = iter->second->deviceInfo();
        //std::string type_str = FFUtils::getPrinterName(dev_info.pid);
        if ((m_filter_placement_default || dev_info.placement == m_filter_placement)
            && (m_filter_status_default || dev_info.status == m_filter_status)
            && (m_filter_types.find(dev_info.pid) != m_filter_types.end())
            && ((m_wlan_btn->GetValue() && !dev_info.lanFlag) || (m_lan_btn->GetValue() && dev_info.lanFlag))) {
            m_device_sizer->Add(iter->second);
            iter->second->Show(true);
        } else {
            iter->second->Show(false);
        }
    }    
    updateDeviceWindowSize();
    m_device_panel->Layout();
    Layout();
    //Fit();
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
    PlacementItemMap backMap;
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
    for (auto& iter : m_status_item_map) {
        iter.second->setValid(false);
    }
    for (auto& iter : m_device_map) {
        std::string status = iter.second->deviceInfo().status;
        auto status_iter = m_status_item_map.find(status);
        if (status_iter == m_status_item_map.end()) {
            auto item = new FilterPopupWindow::StatusItem(m_filter_popup, status);
            item->Bind(EVT_FILTER_ITEM_CLICKED, &DeviceListPanel::onFilterItemClicked, this);
            item->Show(false);
            item->setValid(true);
            m_status_item_map.emplace(std::make_pair(status, item));
        } else {
            status_iter->second->setValid(true);
        }
    }
}

void DeviceListPanel::updateTypeMap()
{
    if (m_filter_popup->IsShown()) {
        m_filter_popup->Dismiss();
    }
    for (auto& iter : m_type_item_map) {
        iter.second->setValid(false);
    }
    m_filter_types.clear();
    for (auto& iter : m_device_map) {
        auto pid = iter.second->deviceInfo().pid;
        auto type_iter = m_type_item_map.find(pid);
        if (type_iter == m_type_item_map.end()) {
            auto item = new FilterPopupWindow::DeviceTypeItem(m_filter_popup, pid, false, false, false);
            item->Bind(EVT_FILTER_ITEM_CLICKED, &DeviceListPanel::onFilterItemClicked, this);
            item->Show(false);
            m_type_item_map.emplace(std::make_pair(pid, item));
        } else {
            type_iter->second->setValid(true);
        }
        if (m_filter_types.find(pid) == m_filter_types.end()) {
            m_filter_types.emplace(pid);
        }
    }
}

void DeviceListPanel::updateFilterTitle()   
{
    if (m_filter_placement_default) {
        m_placement_btn->setText(_L("All"));
    } else {
        m_placement_btn->setText(m_filter_placement);
    }
    Layout();
}

void DeviceListPanel::updateStaticMap()
{
    std::map<std::string, int> statusMap;
    for (const auto& dev : m_device_map) {
        const auto& status = dev.second->deviceInfo().status;
        auto iter = statusMap.find(status);
        if (iter != statusMap.end()) {
            iter->second += 1;   
        } else {
            statusMap.emplace(std::make_pair(status, 1));
        }
    }
    for (auto& iter : m_device_stat_map) {
        iter.second->setCount(0);
    }
    for (const auto& iter : statusMap) {
        auto it = m_device_stat_map.find(iter.first);
        if (it != m_device_stat_map.end()) {
            it->second->setCount(iter.second);
            m_device_stat_map.emplace(std::make_pair(iter.first, it->second));
        } else {
            auto item = new DeviceStaticItemPanel(m_device_panel, iter.first, iter.second);
            item->Show(false);
            m_device_stat_map.emplace(std::make_pair(iter.first, item));
        }
    }
    //for (auto& iter : m_device_stat_map) {
    //    iter.second->Show(iter.second->getCount() > 0);
    //}
}

void DeviceListPanel::updateDeviceSizer()
{
    if (m_simple_book->GetSelection() == 0) {
        return;
    }
    if (m_static_btn->GetValue()) {
        Freeze();
        m_device_sizer->Clear();
        for (const auto& iter : m_device_map) {
            iter.second->Show(false);
        }
        for (const auto& iter : m_device_stat_map) {
            if (iter.second->getCount() > 0) {
                m_device_sizer->Add(iter.second);
                iter.second->Show(true);
            }
        }
        updateDeviceWindowSize();
        m_device_panel->Layout();
        Layout();
        //Fit();
        Thaw();
    } else {
        filterDeviceList();
    }
    if (IsShownOnScreen()) {
        Refresh();
    }
}

void DeviceListPanel::updateDeviceWindowSize()
{
    int count = 0;
    count = m_device_sizer->GetItemCount();
    int width = FromDIP(5 * 220 + 4 * 30);
    int hcnt = count / 5;
    if (hcnt * 5 < count) {
        hcnt += 1;
    }
    if (hcnt == 0) {
        hcnt = 1;
    }
    int height = FromDIP(hcnt * 205 + (hcnt - 1) * 30);
    
    auto last_min = m_device_panel->GetMinSize();
    if (last_min.x != width || last_min.y != height) {
        int space = FromDIP(30);
        m_device_panel->SetMinSize(wxSize(width, height));
        auto sz = m_device_scrolled_window->GetSize();
        sz.x -= 2 * space;
        sz.y -= 2 * space;        
        if (width <= sz.x && height <= sz.x) {
            m_device_panel->SetSize(sz);
        } else {
            m_device_panel->SetSize(width, height);
        }
        m_device_scrolled_window->SetVirtualSize(wxSize(width+2*space, height+2*space));
        m_device_panel->Layout();
    }    
    //Layout();
    //Fit();
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
        unsigned short pid = (unsigned short)event.GetInt();
        FilterPopupWindow::DeviceTypeItem* item = (FilterPopupWindow::DeviceTypeItem*)event.GetEventObject();
        if (item) {
            bool check = item->isChecked();
            auto iter = m_filter_types.find(pid);
            if (check) {
                if (iter == m_filter_types.end()) {
                    m_filter_types.emplace(pid);
                }
            } else {
                if (iter != m_filter_types.end()) {
                    m_filter_types.erase(iter);
                }
            }
        }
        /*auto iter = m_filter_types.find(pid);
        if (iter == m_filter_types.end()) {
            m_filter_types.emplace(pid);
        } else {
            m_filter_types.erase(iter);
        }*/
    }
    filterDeviceList();
    updateFilterTitle();
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
        FilterPopupWindow::StatusItem* last_item = nullptr;
        for (auto& iter : m_status_item_map) {
            if (iter.second->isValid()) {
                iter.second->setBottomCornerRound(false);
                iter.second->setSelect(!m_filter_status_default && (iter.first == m_filter_status));
                m_filter_popup->AddItem(iter.second);
                last_item = iter.second;
            }
        }
        if (last_item) {
            last_item->setBottomCornerRound(true);
        }
        pos = m_status_btn->ClientToScreen(wxPoint(0, 0));
        pos.y += m_status_btn->GetSize().y + 2;
    } else if (event.GetEventObject() == m_type_btn) {
        m_filter_popup_type = Filter_Popup_Type_Device_Type;
        FilterPopupWindow::DeviceTypeItem* last_item = nullptr;
        for (auto& iter : m_type_item_map) {
            if (iter.second->isValid()) {
                iter.second->setBottomCornerRound(false);
                iter.second->setChecked(m_filter_types.find(iter.first) != m_filter_types.end());
                m_filter_popup->AddItem(iter.second);
                last_item = iter.second;
            }
        }
        if (last_item) {
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
    event.Skip();
}

void DeviceListPanel::onStaticModeToggled(wxCommandEvent &event)
{
    updateDeviceSizer();
    event.Skip();
}

void DeviceListPanel::copyDeviceInfo(DeviceInfoItemPanel::DeviceInfo& dest, const DeviceInfoItemPanel::DeviceInfo& source)
{
    //if (source.status != "offline" || source.lanFlag) {
    dest.conn_id = source.conn_id;
    dest.lanFlag = source.lanFlag;
    //}
    if (!source.name.empty()) dest.name = source.name;
    if (source.pid > 0) dest.pid = source.pid;
    if (!source.placement.empty()) dest.placement = source.placement;
    if (!source.status.empty()) {
        dest.status = source.status;
    }
    if (dest.conn_id < 0) {
        dest.status = "offline";
    }
}

void DeviceListPanel::onDeviceListUpdated(DeviceListUpdateEvent& event)
{
//    bool is_show_on_screen = IsShownOnScreen();
    DeviceCacheData device_data;
    device_data.dev_id = event.GetDeviceId();
    device_data.op = event.GetOperator();

    int conn_id = event.GetConnectionId();
    bool valid = getDeviceInfo(device_data.device_info, conn_id);
    auto iter = m_device_data_cached.find(device_data.dev_id);
    if (iter == m_device_data_cached.end()) {
        DeviceKey key(generateNewPriorityId(), device_data.dev_id, device_data.device_info.name);
        m_device_data_cached.emplace(std::make_pair(key, device_data));
    } else {
        auto old_data = iter->second;
        if (DeviceListUpdateEvent::UpdateType::UpdateType_Add == device_data.op) {
            if (DeviceListUpdateEvent::UpdateType::UpdateType_Add == old_data.op
                || DeviceListUpdateEvent::UpdateType::UpdateType_Update == old_data.op) {
                iter->second.op = DeviceListUpdateEvent::UpdateType::UpdateType_Update ;
                copyDeviceInfo(iter->second.device_info, device_data.device_info);
            } else if (DeviceListUpdateEvent::UpdateType::UpdateType_Remove == old_data.op) {
                m_device_data_cached.erase(iter);
            }
        } else if (DeviceListUpdateEvent::UpdateType::UpdateType_Remove == device_data.op) {
            if (DeviceListUpdateEvent::UpdateType::UpdateType_Add == old_data.op) {
                m_device_data_cached.erase(iter);
            } else if (DeviceListUpdateEvent::UpdateType::UpdateType_Update == old_data.op
                || DeviceListUpdateEvent::UpdateType::UpdateType_Remove == old_data.op) {
                copyDeviceInfo(iter->second.device_info, device_data.device_info);
                iter->second.op = DeviceListUpdateEvent::UpdateType::UpdateType_Remove;
            }
        } else if (DeviceListUpdateEvent::UpdateType::UpdateType_Update == device_data.op) {
            if (DeviceListUpdateEvent::UpdateType::UpdateType_Add == old_data.op) {
                copyDeviceInfo(iter->second.device_info, device_data.device_info);
            } else if (DeviceListUpdateEvent::UpdateType::UpdateType_Remove == old_data.op) {
                iter->second.op = DeviceListUpdateEvent::UpdateType::UpdateType_Update;
            } else if (DeviceListUpdateEvent::UpdateType::UpdateType_Update == old_data.op) {
                if (valid) {                    
                    copyDeviceInfo(iter->second.device_info, device_data.device_info);
                } else {
                    iter->second.device_info.conn_id = -1;
                    iter->second.device_info.status = "offline";
                }
                 iter->second.op = DeviceListUpdateEvent::UpdateType::UpdateType_Update; 
            }
        }
    }

    if (/*IsShownOnScreen() && */!m_refresh_timer.IsRunning()) {
        m_refresh_timer.StartOnce(1000);
    }
    event.Skip();

    //if ()
    //m_device_income_cached
}

void DeviceListPanel::onRefreshTimeout(wxTimerEvent& event)
{
    updateDeviceList();
    event.Skip();
    
#if 0
    std::string dev_id = event.GetDeviceId();
    DeviceListUpdateEvent::UpdateType op = event.GetOperator();
    int conn_id = event.GetConnectionId();
    bool refresh_flag = true;
    if (DeviceListUpdateEvent::UpdateType::UpdateType_Add == op) {
        DeviceInfoItemPanel::DeviceInfo dev_info;
        if (getDeviceInfo(dev_info, conn_id)) {
            auto info_iter = m_device_map.find(dev_id);
            if (info_iter == m_device_map.end()) {
                DeviceKey key(generateNewPriorityId(), dev_id, dev_info.name);
                DeviceInfoItemPanel* info_item = new DeviceInfoItemPanel(m_device_panel, dev_info, this);
                m_device_map.emplace(std::make_pair(key, info_item));
            } else if (dev_info.status != "offline" || dev_info.lanFlag) {
                info_iter->second->updateInfo(dev_info);
                refresh_flag = false;
            }
        }
    } else if (DeviceListUpdateEvent::UpdateType::UpdateType_Remove == op) {
        auto iter = m_device_map.find(dev_id);
        if (iter != m_device_map.end()) {
            iter->second->Destroy();
            m_device_map.erase(iter);
        }
        updatePriorityId();
    } else if (DeviceListUpdateEvent::UpdateType::UpdateType_Update == op) {
        DeviceInfoItemPanel::DeviceInfo dev_info;
        auto info_iter = m_device_map.find(dev_id);
        if (getDeviceInfo(dev_info, conn_id)) {
            if (info_iter == m_device_map.end()) {
                DeviceKey key(generateNewPriorityId(), dev_id, dev_info.name);
                DeviceInfoItemPanel* info_item = new DeviceInfoItemPanel(m_device_panel, dev_info, this);
                m_device_map.emplace(std::make_pair(key, info_item));
            } else if (dev_info.status != "offline" || dev_info.lanFlag) {
                info_iter->second->updateInfo(dev_info);
                refresh_flag = false;
            }
        } else if (info_iter != m_device_map.end()) {
            dev_info = info_iter->second->deviceInfo();
            dev_info.lanFlag = true;   // LAN
            dev_info.conn_id = -1;
            dev_info.status = "offline";
            info_iter->second->updateInfo(dev_info);
            refresh_flag = false;
        }
    }
    //std::sort(m_device_map.begin(), m_device_map.end(), deviceKeySortFunc);
    if (refresh_flag) {
        m_filter_placement_default = true;
        m_filter_status_default = true;
        m_simple_book->ChangeSelection(m_device_map.empty() ? 0 : 1);
        updateFilterMap();
        //updateDeviceWindowSize();
        updateDeviceSizer();
    }
    updateStaticMap();
    event.Skip();
#endif
}

void DeviceListPanel::updateDeviceList()
{
    if (m_device_data_cached.empty()) {
        return;
    }

    DeviceKeySet device_key_set;
    for (const auto& iter : m_device_data_cached) {
        device_key_set.emplace(iter.first);
    }
    bool refresh_flag = false;
    for (const auto& it : device_key_set) {
        const std::string& dev_id = it.dev_id;
        auto cache_data = m_device_data_cached[it];
        DeviceListUpdateEvent::UpdateType op = cache_data.op;
        auto dev_info = cache_data.device_info;
        if (DeviceListUpdateEvent::UpdateType::UpdateType_Add == op) {
            auto info_iter = m_device_map.find(dev_id);
            if (info_iter == m_device_map.end()) {
                DeviceInfoItemPanel* info_item = new DeviceInfoItemPanel(m_device_panel, dev_info, this);
                m_device_map.emplace(std::make_pair(it, info_item));
                refresh_flag = true;
            } else {
                auto _dev_info = info_iter->second->deviceInfo();
                if (dev_info.status != "offline" || dev_info.lanFlag) {
                    copyDeviceInfo(_dev_info, dev_info);
                }
                info_iter->second->updateInfo(_dev_info);
            }
        } else if (DeviceListUpdateEvent::UpdateType::UpdateType_Remove == op) {
            auto iter = m_device_map.find(dev_id);
            if (iter != m_device_map.end()) {
                iter->second->Destroy();
                m_device_map.erase(iter);
                refresh_flag = true;
            }
        } else if (DeviceListUpdateEvent::UpdateType::UpdateType_Update == op) {
            auto info_iter = m_device_map.find(dev_id);
            if (info_iter == m_device_map.end()) {
                DeviceInfoItemPanel* info_item = new DeviceInfoItemPanel(m_device_panel, dev_info, this);
                m_device_map.emplace(std::make_pair(it, info_item));
            } else {
                auto _dev_info = info_iter->second->deviceInfo();
                copyDeviceInfo(_dev_info, dev_info);
                info_iter->second->updateInfo(_dev_info);
            }
            refresh_flag = true;
        }
    }
    //std::sort(m_device_map.begin(), m_device_map.end(), deviceKeySortFunc);
    if (refresh_flag) {
        m_filter_placement_default = true;
        m_filter_status_default = true;
        m_simple_book->ChangeSelection(m_device_map.empty() ? 0 : 1);
        updateFilterMap();
        //updateDeviceWindowSize();
        updateDeviceSizer();
    }
    updateStaticMap();
    m_device_data_cached.clear();
}

bool DeviceListPanel::getDeviceInfo(DeviceInfoItemPanel::DeviceInfo& info, int conn_id)
{
    bool valid = false;
    const auto& data = MultiComMgr::inst()->devData(conn_id, &valid);
    if (valid) {
        if (COM_CONNECT_LAN == data.connectMode) {
            std::string dev_id = data.lanDevInfo.serialNumber;
            info.lanFlag = true;
            info.conn_id = conn_id;
            info.name = data.devDetail->name;
            info.pid = data.lanDevInfo.pid;
            info.placement = data.devDetail->location;
            info.status = data.devDetail->status;
        } else if (COM_CONNECT_WAN == data.connectMode && valid && data.devDetail) {
            std::string dev_id = data.wanDevInfo.serialNumber;
            info.lanFlag = false;
            info.conn_id = conn_id;
            info.name = data.wanDevInfo.name;        
            info.pid = data.devDetail->pid;
            info.placement = data.wanDevInfo.location;
            info.status = data.wanDevInfo.status;
        }
        if (info.status.empty()) info.status = "offline";
    }
    return valid;
}

void DeviceListPanel::updateDeviceInfo(const std::string& dev_id, const DeviceInfoItemPanel::DeviceInfo& info)
{
    bool status_changed = false;
    auto iter = m_device_map.find(dev_id);
    if (iter != m_device_map.end()) {
        auto dev_info = iter->second->deviceInfo();        
        if (info.lanFlag || info.status != "offline" || !dev_info.lanFlag) {
            if (dev_info.status != info.status) {
                status_changed = true;
            }
            if (status_changed || dev_info.conn_id != info.conn_id || dev_info.lanFlag != info.lanFlag
                || dev_info.name != info.name || dev_info.pid != info.pid || dev_info.placement != info.placement) {
                iter->second->updateInfo(info);
            }
            if (status_changed) {
                updateStatusMap();
                updateStaticMap();
                if (m_static_btn->GetValue()) {
                    updateDeviceSizer();
                }
            }
        }
    }
}

void DeviceListPanel::onComDevDetailUpdate(ComDevDetailUpdateEvent& event)
{
    auto conn_id = event.id;
    bool valid = false;
    const auto& data = MultiComMgr::inst()->devData(conn_id, &valid);
    BOOST_LOG_TRIVIAL(info) << "onComDevDetailUpdate: " << data.connectMode << ", " << valid ? "valid" : "invalid";
    if (COM_CONNECT_LAN == data.connectMode && valid) {
        std::string dev_id = data.lanDevInfo.serialNumber;
        DeviceInfoItemPanel::DeviceInfo info;
        info.lanFlag = true;
        info.conn_id = conn_id;
        info.name = data.devDetail->name;
        info.pid = data.devDetail->pid;
        info.placement = data.devDetail->location;
        info.status = data.devDetail->status;
        updateDeviceInfo(dev_id, info);
        BOOST_LOG_TRIVIAL(info) << "onComDevDetailUpdate: " << info.name << ", " << info.placement << ", " << info.status;
    }
    flush_logs();
    event.Skip();
}

void DeviceListPanel::onComWanDeviceInfoUpdate(ComWanDevInfoUpdateEvent& event)
{
    auto conn_id = event.id;
    bool valid = false;
    const auto& data = MultiComMgr::inst()->devData(conn_id, &valid);
    BOOST_LOG_TRIVIAL(error) << "onComWanDeviceInfoUpdate: " << data.connectMode << ", " << valid ? "valid" : "invalid";
    if (COM_CONNECT_WAN == data.connectMode && valid && data.devDetail) {
        std::string dev_id = data.wanDevInfo.serialNumber;
        DeviceInfoItemPanel::DeviceInfo info;
        info.lanFlag = false;
        info.conn_id = conn_id;
        info.name = data.wanDevInfo.name;
        info.pid = data.devDetail->pid;
        info.placement = data.wanDevInfo.location;
        info.status = data.wanDevInfo.status;
        updateDeviceInfo(dev_id, info);
        BOOST_LOG_TRIVIAL(error) << "onComDevDetailUpdate: " << info.name << ", " << info.placement << ", " << info.status;
    }
    flush_logs();
    event.Skip();
}

int DeviceListPanel::generateNewPriorityId()
{
    return ++m_last_priority_id;
}

void DeviceListPanel::updatePriorityId()
{
    int max = 0;
    for (const auto& iter : m_device_map) {
        if (iter.first.priority > max) {
            max = iter.first.priority;
        }
    }
    m_last_priority_id = max;
}

} // GUI
} // Slic3r