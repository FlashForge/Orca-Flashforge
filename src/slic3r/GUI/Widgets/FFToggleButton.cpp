#include "FFToggleButton.hpp"


FFToggleButton::FFToggleButton(wxWindow* parent, const wxString& label/*= ""*/, wxWindowID id/*= wxID_ANY*/)
	: wxToggleButton(parent, id, label, wxDefaultPosition, wxDefaultSize, wxNO_BORDER)
	, m_hoverFlag(false)
	, m_pressFlag(false)
	, m_normalColor("#999999")
	, m_normalHoverColor("#95C5FF")
	, m_normalPressColor("#328DFB")
	, m_selectColor("#328DFB")
	, m_selectHoverColor("#116FDF")
	, m_selectPressColor("#999999")
{
	Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent& e) { m_hoverFlag = true; updateState(); e.Skip(); });
	Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& e) { m_hoverFlag = false; m_pressFlag = false; updateState(); e.Skip(); });
	Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) { m_pressFlag = true; updateState(); e.Skip(); });
	Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) { m_pressFlag = false; updateState(); e.Skip(); });
	Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent& e) { updateState(); e.Skip(); });
	updateState();
}

void FFToggleButton::SetValue(bool state)
{
	wxToggleButton::SetValue(state);
	updateState();
}

void FFToggleButton::updateState()
{
	SetBackgroundColour(*wxWHITE);
	if (GetValue()) {
		if (m_pressFlag) {
			SetForegroundColour(m_selectPressColor);
		} else if (m_hoverFlag) {
			SetForegroundColour(m_selectHoverColor);
		} else {
			SetForegroundColour(m_selectColor);
		}
	} else {
		if (m_pressFlag) {
			SetForegroundColour(m_normalPressColor);
		} else if (m_hoverFlag) {
			SetForegroundColour(m_normalHoverColor);
		} else {
			SetForegroundColour(m_normalColor);
		}
	}
	Update();
}
