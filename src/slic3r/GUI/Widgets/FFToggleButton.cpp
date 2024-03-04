#include "FFToggleButton.hpp"


FFToggleButton::FFToggleButton(wxWindow* parent, const wxString& label/*= ""*/, wxWindowID id/*= wxID_ANY*/)
	: wxToggleButton(parent, id, label, wxDefaultPosition, wxDefaultSize, wxNO_BORDER)
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

void FFToggleButton::setNormalColor(const wxColour& color)
{
	m_normalColor = color;	
}

void FFToggleButton::setNormalHoverColor(const wxColour& color)
{
	m_normalHoverColor = color;
}

void FFToggleButton::setNormalPressColor(const wxColour& color)
{
	m_normalPressColor = color;
}

void FFToggleButton::setSelectColor(const wxColour& color)
{
	m_selectColor = color;
}

void FFToggleButton::setSelectHoverColor(const wxColour& color)
{
	m_selectHoverColor = color;
}

void FFToggleButton::setSelectPressColor(const wxColour& color)
{
	m_selectPressColor = color;
}

void FFToggleButton::updateState()
{
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
