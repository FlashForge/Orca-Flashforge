#include "DeviceFilterPopup.hpp"
#include <boost/log/trivial.hpp>
#include <wx/graphics.h>
#include "libslic3r/Utils.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Widgets/FFToggleButton.hpp"
#include "slic3r/GUI/Widgets/FFCheckBox.hpp"
#include "slic3r/GUI/wxExtensions.hpp"
#include "slic3r/GUI/FFUtils.hpp"


namespace Slic3r {
namespace GUI {


wxDEFINE_EVENT(EVT_DEVICE_FILTER_ITEM_CLICKED, DeviceFilterEvent);
DeviceFilterItem::DeviceFilterItem(wxWindow* parent, const wxString& label, bool top_corner_round/*=false*/, bool bottom_corner_round/*=false*/)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER)
    , m_top_corner_round(top_corner_round)
    , m_bottom_corner_round(bottom_corner_round)
{
    SetMaxSize(wxSize(-1, FromDIP(30)));
    SetSize(wxSize(-1, FromDIP(30)));
    m_main_sizer = new wxBoxSizer(wxHORIZONTAL);
    SetSizer(m_main_sizer);
    
    m_text = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_main_sizer->AddSpacer(FromDIP(15));
    m_main_sizer->Add(m_text, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, FromDIP(5));
    m_main_sizer->AddSpacer(FromDIP(15));

    SetLabel(label);
    Layout();
    Fit();

    Bind(wxEVT_PAINT, &DeviceFilterItem::onPaint, this);
    Bind(wxEVT_SHOW, [this](auto& e) {
        if (!e.IsShown()) {m_hover_flag = false; m_press_flag = false;}
        });
#ifndef __WXMAC__
    Bind(wxEVT_ENTER_WINDOW, &DeviceFilterItem::onEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &DeviceFilterItem::onLeave, this);
    Bind(wxEVT_LEFT_DOWN, &DeviceFilterItem::onMouseDown, this);
    Bind(wxEVT_LEFT_UP, &DeviceFilterItem::onMouseUp, this);
    m_text->Bind(wxEVT_ENTER_WINDOW, &DeviceFilterItem::onEnter, this);
    m_text->Bind(wxEVT_LEAVE_WINDOW, &DeviceFilterItem::onLeave, this);
    m_text->Bind(wxEVT_LEFT_DOWN, &DeviceFilterItem::onMouseDown, this);
    m_text->Bind(wxEVT_LEFT_UP, &DeviceFilterItem::onMouseUp, this);
#endif
}

DeviceFilterItem::~DeviceFilterItem()
{
}

void DeviceFilterItem::SetLabel(const wxString& label)
{
    wxWindow::SetLabel(label);
    wxString str = FFUtils::elideString(m_text, label, FromDIP(170));
    m_text->SetLabel(str);
    
    messureSize();
    Layout();
}

void DeviceFilterItem::messureSize()
{
    int min_width = m_text->GetTextExtent(m_text->GetLabel()).x + FromDIP(35);
    if (min_width < FromDIP(80)) {
        min_width = FromDIP(80);
    }
    SetMinSize(wxSize(min_width, FromDIP(30)));
    SetSize(wxSize(min_width, FromDIP(30)));
}

void DeviceFilterItem::SetSelect(bool select)
{
    if (m_select_flag != select) {
        m_select_flag = select;
        Refresh();
    }
}

void DeviceFilterItem::SetHover(bool hover)
{
    if (m_hover_flag != hover) {
        m_hover_flag = hover;
        Refresh();
    }
}

void DeviceFilterItem::SetPressed(bool pressed, bool hit)
{
    if (m_press_flag != pressed) {
        m_press_flag = pressed;
        Refresh();
        if (hit) {
            if (pressed) {
                mouseDownEvent();
            } else {
                mouseUpEvent();
            }
        }
    }
}

void DeviceFilterItem::SetTopCornerRound(bool round)
{
    if (m_top_corner_round != round) {
        m_top_corner_round = round;
        Refresh();
    }
}

void DeviceFilterItem::SetBottomCornerRound(bool round)
{
    if (m_bottom_corner_round != round) {
        m_bottom_corner_round = round;
        Refresh();
    }
}

void DeviceFilterItem::onPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    auto sz = GetSize();
    dc.SetPen(*wxTRANSPARENT_PEN);
    //if (!m_top_corner_round || !m_bottom_corner_round) {
    //if (GetParent()) {
    //    dc.SetBrush(GetParent()->GetBackgroundColour());
    //    dc.DrawRectangle(0, 0, sz.x, sz.y);
    // }
    wxColour color("#ffffff"), fcolor("#333333");
    if (m_press_flag) {
        color = wxColour("#D9EAFF");
        //fcolor = wxColour("#ffffff");
    } else if (m_hover_flag) {
        color = wxColour("#D9EAFF");
    } else if (m_select_flag) {
        color = wxColour("#328DFB");
        fcolor = wxColour("#ffffff");
    }
    dc.SetBrush(color);
    //if (!m_top_corner_round && !m_bottom_corner_round) {
    //    dc.DrawRectangle(0, 0, sz.x, sz.y);
    //} else if (m_top_corner_round && m_bottom_corner_round) {
    //    dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, 6);
    //} else if (m_top_corner_round) {
    //    dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, 6);
    //    dc.DrawRectangle(0, 6, sz.x, sz.y);
    //} else if (m_bottom_corner_round) {
    //    dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, 6);
    //    dc.DrawRectangle(0, 0, sz.x, 6);
    //}
    dc.DrawRectangle(0, 0, sz.x, sz.y);
    updateChildrenBackground(color);
    m_text->SetForegroundColour(fcolor);
}

#ifndef __WXMAC__
wxPoint DeviceFilterItem::convertEventPoint(const wxMouseEvent& event)
{
    wxPoint pnt = event.GetPosition();
    if (event.GetId() == m_text->GetId()) {
        pnt += m_text->GetPosition();
    }
    return pnt;
}

void DeviceFilterItem::onEnter(wxMouseEvent& event)
{
    SetHover(true);
    event.Skip();
}

void DeviceFilterItem::onLeave(wxMouseEvent& event)
{
    auto rect = GetRect();
    wxPoint pnt = convertEventPoint(event);
    if (!wxRect(GetSize()).Contains(pnt)) {
        SetHover(false);
    }
    event.Skip();
}

void DeviceFilterItem::onMouseDown(wxMouseEvent& event)
{
    wxPoint pnt = convertEventPoint(event);
    CaptureMouse();
    SetPressed(true, true);
    event.Skip();
}

void DeviceFilterItem::onMouseUp(wxMouseEvent& event)
{
    wxPoint pnt = convertEventPoint(event);
    if (wxRect(GetSize()).Contains(pnt)) {
        SetPressed(false, true);
    } else {
        SetPressed(false, false);
    }
    ReleaseMouse();
    event.Skip();
}
#endif /* __WXMAC__ */

void DeviceFilterItem::updateChildrenBackground(const wxColour& color)
{
    m_text->SetBackgroundColour(color);
}

void DeviceFilterItem::sendEvent(const wxString& full_data, const wxString& trim_data, int int_data)
{
    DeviceFilterEvent event(EVT_DEVICE_FILTER_ITEM_CLICKED, GetId(), full_data, trim_data, int_data, this);
    event.SetEventObject(this);
    wxPostEvent(this, event);
}

void DeviceFilterItem::mouseUpEvent()
{
    sendEvent(GetLabel(), m_text->GetLabel(), 0);
}


DeviceStatusFilterItem::DeviceStatusFilterItem(wxWindow* parent, const wxString& status,
    bool top_corner_round/* = false*/, bool bottom_corner_round/* = false*/)
    : DeviceFilterItem(parent, wxEmptyString, top_corner_round, bottom_corner_round)
    , m_status(status)
{
    SetStatus(m_status);
}


void DeviceStatusFilterItem::mouseUpEvent()
{
    sendEvent(m_status, m_text->GetLabel(), 0);
}

void DeviceStatusFilterItem::SetStatus(const wxString& status)
{
    m_status = status;
    SetLabel(m_status);
}


DeviceTypeFilterItem::DeviceTypeFilterItem(wxWindow* parent, unsigned short pid, bool checked/* = false*/, bool top_corner_round/* = false*/, bool bottom_corner_round/* = false*/)
    : DeviceFilterItem(parent, wxEmptyString, top_corner_round, bottom_corner_round)
    , m_pid(pid)
{
    m_main_sizer->Clear();
    m_bitmap = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize(FromDIP(16), FromDIP(16)));
    m_bitmap->SetMinSize(wxSize(FromDIP(16), FromDIP(16)));
    m_bitmap->SetMinSize(wxSize(FromDIP(16), FromDIP(16)));

    //m_check_box = new FFCheckBox(this);
    m_main_sizer->AddSpacer(FromDIP(15));
    m_main_sizer->Add(m_bitmap, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxTOP | wxBOTTOM, FromDIP(5));
    m_main_sizer->Add(m_text, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, FromDIP(5));
    m_main_sizer->AddSpacer(FromDIP(15));
    
    SetLabel(FFUtils::getPrinterName(m_pid));
    updateBitmap();
    //m_check_box->SetValue(checked);

 #ifndef __WXMAC__
    m_bitmap->Bind(wxEVT_LEFT_DOWN, &DeviceTypeFilterItem::onMouseDown, this);
    m_bitmap->Bind(wxEVT_LEFT_UP, &DeviceTypeFilterItem::onMouseUp, this);
#endif
}

bool DeviceTypeFilterItem::IsChecked() const
{
    return m_check_flag;
    //return m_check_box->GetValue();
}

void DeviceTypeFilterItem::SetChecked(bool checked)
{
    if (m_check_flag != checked) {
        m_check_flag = checked;
        updateBitmap();
    }
    //m_check_box->SetValue(checked);
}

void DeviceTypeFilterItem::updateChildrenBackground(const wxColour& color)
{
    m_bitmap->SetBackgroundColour(color);
    DeviceFilterItem::updateChildrenBackground(color);
}

void DeviceTypeFilterItem::mouseDownEvent()
{
    SetChecked(!IsChecked());
    //m_check_box->SetValue(!m_check_box->GetValue());
    sendEvent("", "", m_pid);
}

void DeviceTypeFilterItem::messureSize()
{
    //int min_width = m_check_box->GetSize().x + m_text->GetTextExtent(GetLabel()).x + FromDIP(40);
    int min_width = m_bitmap->GetSize().x + m_text->GetTextExtent(GetLabel()).x + FromDIP(40);
    SetMinSize(wxSize(min_width, FromDIP(30)));
    SetSize(wxSize(min_width, FromDIP(30)));
}

void DeviceTypeFilterItem::updateBitmap()
{
    if (m_check_flag) {
        m_bitmap->SetBitmap(create_scaled_bitmap("ff_check_on", 0, 16));
    } else {
        m_bitmap->SetBitmap(create_scaled_bitmap("ff_check_off", 0, 16));
    }
}

#ifdef __WXMAC__
DeviceFilterPopupWindow::DeviceFilterPopupWindow(wxWindow* parent)
    : FFPopupWindow(parent)
    , m_sizer(new wxBoxSizer(wxVERTICAL))
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__
    SetBackgroundColour(wxColour("#c1c1c1"));
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_sizer, 1, wxEXPAND | wxALL, 1);
    SetSizer(sizer);
}

DeviceFilterPopupWindow::~DeviceFilterPopupWindow()
{
}

void DeviceFilterPopupWindow::Create()
{
    m_sizer->Clear();
    int max_width = 0;
    int height = m_items.size() * FromDIP(30);
    for (auto btn : m_items) {
        max_width = std::max(btn->GetMinSize().x, max_width);
    }

    for (auto btn : m_items) {
        btn->SetSize(max_width, FromDIP(30));
        btn->Layout();
        m_sizer->Add(btn, 0, wxEXPAND);
    }
    SetSize(wxSize(max_width+2, height+2));
    Layout();
    Refresh();
}

void DeviceFilterPopupWindow::Popup(wxWindow* focus/*=nullptr*/)
{
    Create();
    //wxGraphicsPath path = wxGraphicsRenderer::GetDefaultRenderer()->CreatePath();
    //wxSize size = GetSize();
    //path.AddRoundedRectangle(0, 0, size.x, size.y, 6);
    //SetShape(path);
    FFPopupWindow::Popup(focus);
}

void DeviceFilterPopupWindow::OnDismiss()
{
    ClearItems();
    FFPopupWindow::OnDismiss();
}

void DeviceFilterPopupWindow::AddItem(DeviceFilterItem* item)
{
    item->Show(true);
    m_items.emplace_back(item);
}

void DeviceFilterPopupWindow::ClearItems()
{
    for (auto& iter : m_items) {
        iter->Show(false);
    }
    m_items.clear();
    m_sizer->Clear();
}

void DeviceFilterPopupWindow::ProcessLeftDown(const wxPoint& pnt)
{
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
}

void DeviceFilterPopupWindow::ProcessLeftUp(const wxPoint& pnt)
{
    for (auto& it : m_items) {
        if (it->IsPressed() && it->GetRect().Contains(pnt)) {
            it->SetPressed(false, true);
            break;
        } else {
            it->SetPressed(false, false);
        }
    }
}

void DeviceFilterPopupWindow::ProcessMotion(const wxPoint& pnt)
{
    for (auto& it : m_items) {
        it->SetHover(false);
    }
    for (auto& it : m_items) {
        if (it->GetRect().Contains(pnt)) {
            it->SetHover(true);
            break;
        }
    }
}
#else

DeviceFilterPopupWindow::DeviceFilterPopupWindow(wxWindow* parent)
    : PopupWindow(parent, wxBORDER_NONE | wxPU_CONTAINS_CONTROLS | wxFRAME_SHAPED)
    , m_sizer(new wxBoxSizer(wxVERTICAL))
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__
    SetBackgroundColour(wxColour("#c1c1c1"));
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_sizer, 0, wxEXPAND | wxALL, 1);
    SetSizer(sizer);
}

DeviceFilterPopupWindow::~DeviceFilterPopupWindow()
{    
}

void DeviceFilterPopupWindow::Create()
{
    m_sizer->Clear();
    int max_width = 0;
    int height = m_items.size() * FromDIP(30);
    for (auto btn : m_items) {
        max_width = std::max(btn->GetMinSize().x, max_width);
    }

    for (auto btn : m_items) {
        btn->SetSize(max_width, FromDIP(30));
        btn->Layout();
        m_sizer->Add(btn, 0, wxEXPAND);
    }
    SetSize(wxSize(max_width+2, height+2));
    Layout();
    Refresh();
}

void DeviceFilterPopupWindow::Popup(wxWindow* focus/* = nullptr*/)
{
    Create();
    //wxGraphicsPath path = wxGraphicsRenderer::GetDefaultRenderer()->CreatePath();
    //wxSize size = GetSize();
    //path.AddRoundedRectangle(0, 0, size.x, size.y, 8);
    //SetShape(path);
    if (focus) {
        wxPoint pos = focus->ClientToScreen(wxPoint(0, focus->GetSize().y + 2));
        Move(pos);
    }
    PopupWindow::Popup();
}

void DeviceFilterPopupWindow::OnDismiss()
{
    ClearItems();
}

void DeviceFilterPopupWindow::AddItem(DeviceFilterItem* item)
{
    item->Show(true);
    m_items.emplace_back(item);
}

void DeviceFilterPopupWindow::ClearItems()
{
    for (auto& iter : m_items) {
        iter->Show(false);
    }
    m_items.clear();
    m_sizer->Clear();
}

void DeviceFilterPopupWindow::onPaint(wxPaintEvent& event)
{
    auto sz = GetSize();
    wxPaintDC dc(this);
    dc.SetBrush(wxColour("#c1c1c1"));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(0, 0, sz.x, sz.y);
}

bool DeviceFilterPopupWindow::ProcessLeftDown(wxMouseEvent &event)
{
    return PopupWindow::ProcessLeftDown(event);
}

#endif /* __WXMAC__ */

} // GUI
} // Slic3r

