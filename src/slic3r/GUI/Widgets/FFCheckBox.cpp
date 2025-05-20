#include "FFCheckBox.hpp"

#include "../wxExtensions.hpp"

FFCheckBox::FFCheckBox(wxWindow *parent, int id)
    : wxBitmapToggleButton(parent, id, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
    , m_on(this, "ff_check_on", 16)
    , m_off(this, "ff_check_off", 16)
    , m_on_focused(this, "ff_check_on_hover", 16)
    , m_off_focused(this, "ff_check_off_hover", 16)
    , m_on_pressed(this, "ff_check_on_press", 16)
    , m_off_pressed(this, "ff_check_off_press", 16)
{
	//SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
	if (parent)
        SetBackgroundColour(*wxWHITE);
		//SetBackgroundColour(parent->GetBackgroundColour());
	Bind(wxEVT_TOGGLEBUTTON, [this](auto& e) { update(); e.Skip(); });
#ifdef __WXOSX__ // State not fully implement on MacOS
    Bind(wxEVT_SET_FOCUS, &FFCheckBox::updateBitmap, this);
    Bind(wxEVT_KILL_FOCUS, &FFCheckBox::updateBitmap, this);
    Bind(wxEVT_ENTER_WINDOW, &FFCheckBox::updateBitmap, this);
    Bind(wxEVT_LEAVE_WINDOW, &FFCheckBox::updateBitmap, this);
#endif
	SetSize(m_on.GetBmpSize());
	SetMinSize(m_on.GetBmpSize());
	update();
}

void FFCheckBox::SetValue(bool value)
{
	wxBitmapToggleButton::SetValue(value);
	update();
}

void FFCheckBox::Rescale()
{
    m_on.msw_rescale();
    m_off.msw_rescale();
    m_on_pressed.msw_rescale();
    m_off_pressed.msw_rescale();
    m_on_focused.msw_rescale();
    m_off_focused.msw_rescale();
    SetSize(m_on.GetBmpSize());
	update();
}

void FFCheckBox::update()
{
    bool value = GetValue();
	SetBitmap((GetValue() ? m_on : m_off).bmp());
	SetBitmapLabel((GetValue() ? m_on : m_off).bmp());
    SetBitmapDisabled((GetValue() ? m_on : m_off).bmp());
    SetBitmapPressed((GetValue() ? m_on_pressed : m_off_pressed).bmp());
#ifdef __WXMSW__
    SetBitmapFocus((GetValue() ? m_on_focused : m_off_focused).bmp());
#endif
    SetBitmapCurrent((GetValue() ? m_on_focused : m_off_focused).bmp());
#ifdef __WXOSX__
    wxCommandEvent e(wxEVT_UPDATE_UI);
    updateBitmap(e);
#endif
    Update();
}

#ifdef __WXMSW__

FFCheckBox::State FFCheckBox::GetNormalState() const { return State_Normal; }

#endif


#ifdef __WXOSX__

bool FFCheckBox::Enable(bool enable)
{
    bool result = wxBitmapToggleButton::Enable(enable);
    if (result) {
        m_disable = !enable;
        wxCommandEvent e(wxEVT_ACTIVATE);
        updateBitmap(e);
    }
    return result;
}

wxBitmap FFCheckBox::DoGetBitmap(State which) const
{
    if (m_disable) {
        return wxBitmapToggleButton::DoGetBitmap(State_Disabled);
    }
    if (m_focus) {
        return wxBitmapToggleButton::DoGetBitmap(State_Current);
    }
    return wxBitmapToggleButton::DoGetBitmap(which);
}

void FFCheckBox::updateBitmap(wxEvent & evt)
{
    evt.Skip();
    if (evt.GetEventType() == wxEVT_ENTER_WINDOW) {
        m_hover = true;
    } else if (evt.GetEventType() == wxEVT_LEAVE_WINDOW) {
        m_hover = false;
    } else {
        if (evt.GetEventType() == wxEVT_SET_FOCUS) {
            m_focus = true;
        } else if (evt.GetEventType() == wxEVT_KILL_FOCUS) {
            m_focus = false;
        }
        wxMouseEvent e;
        if (m_hover)	
            OnEnterWindow(e);
        else
            OnLeaveWindow(e);
    }
}
	
#endif
