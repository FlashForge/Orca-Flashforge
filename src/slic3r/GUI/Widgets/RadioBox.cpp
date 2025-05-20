#include "RadioBox.hpp"

#include "../wxExtensions.hpp"

namespace Slic3r { 
namespace GUI {
RadioBox::RadioBox(wxWindow *parent)
    : wxBitmapToggleButton(parent, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE), m_on(this, "radio_on", 18), m_off(this, "radio_off", 18)
{
    // SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
    if (parent) SetBackgroundColour(parent->GetBackgroundColour());
    // Bind(wxEVT_TOGGLEBUTTON, [this](auto& e) { update(); e.Skip(); });
    SetSize(m_on.GetBmpSize());
    SetMinSize(m_on.GetBmpSize());
    update();
}

void RadioBox::SetValue(bool value)
{
    wxBitmapToggleButton::SetValue(value);
    update();
}

bool RadioBox::GetValue()
{
    return wxBitmapToggleButton::GetValue();
}


void RadioBox::Rescale()
{
    m_on.msw_rescale();
    m_off.msw_rescale();
    SetSize(m_on.GetBmpSize());
    update();
}

void RadioBox::update() { SetBitmap((GetValue() ? m_on : m_off).bmp()); }



RadioButton::RadioButton(wxWindow* parent)
    : wxBitmapToggleButton(parent, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
    , m_mode(PaintMode::Normal)
    , m_on_normal(this, "radio_true_normal", 16)
    , m_on_hover(this, "radio_true_hover", 16)
    , m_off_normal(this, "radio_false_normal", 16)
    , m_off_hover(this, "radio_false_hover", 16)
{
    connectEvent();
    Refresh();
}

RadioButton::~RadioButton() {}

void RadioButton::SetValue(bool value)
{
    wxBitmapToggleButton::SetValue(value);
    Refresh();
}

bool RadioButton::GetValue()
{
    return wxBitmapToggleButton::GetValue();
    
}

void RadioButton::paintEvent(wxPaintEvent& event) 
{
    wxPaintDC dc(this);
    switch (m_mode) {
    case RadioButton::Normal: {
        if (GetValue()) {
            dc.DrawBitmap(m_on_normal.bmp(), 0, 0);
        } else {
            dc.DrawBitmap(m_off_normal.bmp(), 0, 0);
        }        
        break;
    }
    case RadioButton::Hover: {
        if (GetValue()) {
            dc.DrawBitmap(m_on_hover.bmp(), 0, 0);
        } else {
            dc.DrawBitmap(m_off_hover.bmp(), 0, 0);
        } 
        break;
    }
    default: break;
    }
}

void RadioButton::OnMouseEnter(wxMouseEvent& event)
{
    m_mode = PaintMode::Hover;
    Refresh();
}

void RadioButton::OnMouseLeave(wxMouseEvent& event)
{
    m_mode = PaintMode::Normal;
    Refresh();
}

void RadioButton::connectEvent()
{
    Bind(wxEVT_PAINT, &RadioButton::paintEvent, this);
    Bind(wxEVT_ENTER_WINDOW, &RadioButton::OnMouseEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &RadioButton::OnMouseLeave, this);
}



}//namespace
}

