#include "RadioBox.hpp"

#include "../wxExtensions.hpp"

#ifdef __WXGTK3__
#include "../GUI_Utils.hpp"
#endif

namespace Slic3r {
namespace GUI {
RadioBox::RadioBox(wxWindow *parent)
    : wxBitmapToggleButton(parent, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
    , m_on(this, "radio_on", 18)
    , m_off(this, "radio_off", 18)
    , m_ban(this, "radio_ban", 18)
{
    // SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
    if (parent) SetBackgroundColour(parent->GetBackgroundColour());
    // Bind(wxEVT_TOGGLEBUTTON, [this](auto& e) { update(); e.Skip(); });
    update();
#ifdef __WXGTK__
    wxSize bestSize = GetBestSize();
    bestSize.IncTo(m_on.GetBmpSize());
    SetSize(bestSize);
    SetMinSize(bestSize);
#else
    SetSize(m_on.GetBmpSize());
    SetMinSize(m_on.GetBmpSize());
#endif
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
    update();
#ifdef __WXGTK__
    wxSize bestSize = GetBestSize();
    bestSize.IncTo(m_on.GetBmpSize());
    SetSize(bestSize);
    SetMinSize(bestSize);
#else
    SetSize(m_on.GetBmpSize());
    SetMinSize(m_on.GetBmpSize());
#endif
}

void RadioBox::update() {
    if (IsEnabled())
    {
        SetBitmap((GetValue() ? m_on : m_off).bmp());
    } else
    {
        SetBitmap(m_ban.bmp());
    }

}



RadioButton::RadioButton(wxWindow* parent)
    : wxBitmapToggleButton(parent, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
    , m_mode(PaintMode::Normal)
    , m_on_normal(this, "radio_true_normal", 16)
    , m_on_hover(this, "radio_true_hover", 16)
    , m_off_normal(this, "radio_false_normal", 16)
    , m_off_hover(this, "radio_false_hover", 16)
{
    connectEvent();
#ifdef __WXGTK__
    // GTK backs wxBitmapToggleButton with a native GtkButton that does not
    // fire wxEVT_PAINT for owner-drawing, so paintEvent() never runs and the
    // widget renders blank. Push bitmaps through SetBitmap*() so GTK renders
    // them, and re-apply on toggle / hover changes.
    SetSize(m_off_normal.GetBmpSize());
    SetMinSize(m_off_normal.GetBmpSize());
#ifdef __WXGTK3__
    // Without this GTK3 pads the 16 px button so the bitmap gets no room and is never drawn
    RemoveButtonBorder(this);
#endif
    update();
    Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &e) { update(); e.Skip(); });
#endif
    Refresh();
}

RadioButton::~RadioButton() {}

void RadioButton::SetValue(bool value)
{
    wxBitmapToggleButton::SetValue(value);
#ifdef __WXGTK__
    update();
#endif
    Refresh();
}

#ifdef __WXGTK__
void RadioButton::update()
{
    const ScalableBitmap &bmp = GetValue()
        ? (m_mode == PaintMode::Hover ? m_on_hover : m_on_normal)
        : (m_mode == PaintMode::Hover ? m_off_hover : m_off_normal);
    SetBitmap(bmp.bmp());
    SetBitmapLabel(bmp.bmp());
    SetBitmapPressed(bmp.bmp());
    SetBitmapCurrent(bmp.bmp());
    SetBitmapDisabled(bmp.bmp());
}
#endif

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
#ifdef __WXGTK__
    update();
#endif
    Refresh();
}

void RadioButton::OnMouseLeave(wxMouseEvent& event)
{
    m_mode = PaintMode::Normal;
#ifdef __WXGTK__
    update();
#endif
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

