#include "FFToggleButton.hpp"


FFToggleButton::FFToggleButton(wxWindow* parent, const wxString& label/*= ""*/, wxWindowID id/*= wxID_ANY*/)
	: wxToggleButton(parent, id, label, wxDefaultPosition, wxDefaultSize, wxNO_BORDER)
{
	if (parent) {
		SetBackgroundColour(parent->GetBackgroundColour());
	}
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


FFBitmapToggleButton::FFBitmapToggleButton(wxWindow* parent, wxWindowID id/*=wxID_ANY*/
		, const wxPoint& pos/*=wxDefaultPosition*/, const wxSize& size/*=wxDefaultSize*/)
	: wxBitmapToggleButton(parent, id, wxNullBitmap, pos, size, wxNO_BORDER)
{
	if (parent) {
		SetBackgroundColour(parent->GetBackgroundColour());
	}
	Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent& e) { m_hoverFlag = true; updateState(); e.Skip(); });
	Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& e) { m_hoverFlag = false; m_pressFlag = false; updateState(); e.Skip(); });
	Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) { m_pressFlag = true; updateState(); e.Skip(); });
	Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) { m_pressFlag = false; updateState(); e.Skip(); });
	Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent& e) { updateState(); e.Skip(); });
}

void FFBitmapToggleButton::SetValue(bool state)
{
	wxToggleButton::SetValue(state);
	updateState();
}

void FFBitmapToggleButton::setNormalBitmap(const wxBitmap& bmp)
{
	m_normalBitmap = bmp;
}

void FFBitmapToggleButton::setNormalHoverBitmap(const wxBitmap& bmp)
{
	m_normalHoverBitmap = bmp;
}

void FFBitmapToggleButton::setNormalPressBitmap(const wxBitmap& bmp)
{
	m_normalPressBitmap = bmp;
}

void FFBitmapToggleButton::setSelectBitmap(const wxBitmap& bmp)
{
	m_selectBitmap = bmp;
}

void FFBitmapToggleButton::setSelectHoverBitmap(const wxBitmap& bmp)
{
	m_selectHoverBitmap = bmp;
}

void FFBitmapToggleButton::setSelectPressBitmap(const wxBitmap& bmp)
{
	m_selectPressBitmap = bmp;
}

void FFBitmapToggleButton::updateState()
{
	if (GetValue()) {
		if (m_pressFlag) {
			SetBitmap(m_selectBitmap);
		} else if (m_hoverFlag) {
			SetBitmap(m_selectHoverBitmap);
		} else {
			SetBitmap(m_selectBitmap);
		}
	} else {
		if (m_pressFlag) {
			SetBitmap(m_normalPressBitmap);
		} else if (m_hoverFlag) {
			SetBitmap(m_normalHoverBitmap);
		} else {
			SetBitmap(m_normalBitmap);
		}
	}
	Update();
}

