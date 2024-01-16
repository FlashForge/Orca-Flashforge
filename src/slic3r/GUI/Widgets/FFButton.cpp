#include "FFButton.hpp"
#include <wx/dcgraph.h>


FFButton::FFButton(wxWindow* parent, wxWindowID id/*= wxID_ANY*/, const wxString& label/*= ""*/,
	int borderRadius/*=4*/, bool borderFlag/* = true*/)
	: wxButton(parent, id, label, wxDefaultPosition, wxDefaultSize, wxNO_BORDER)
	, m_hoverFlag(false)
	, m_pressFlag(false)
	, m_borderFlag(borderFlag)
	, m_borderRadius(borderRadius)
	, m_fontColor("#333333")
	, m_fontHoverColor("#65A79E")
	, m_fontPressColor("#419488")
	, m_fontDisableColor("#dddddd")
	, m_borderColor("#333333")
	, m_borderHoverColor("#65A79E")
	, m_borderPressColor("#419488")
	, m_borderDisableColor("#dddddd")
	, m_bgColor("#ffffff")
	, m_bgHoverColor("#ffffff")
	, m_bgPressColor("#ffffff")
	, m_bgDisableColor("#dddddd")
{
	Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent& e) { m_hoverFlag = true; Update(); e.Skip(); });
	Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& e) { m_hoverFlag = false; m_pressFlag = false; Update(); e.Skip(); });
	Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) { m_pressFlag = true; Refresh(); e.Skip(); });
	Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) { m_pressFlag = false; Update(); e.Skip(); });
	Bind(wxEVT_PAINT, &FFButton::OnPaint, this);
	updateState();
}

void FFButton::SetFontColor(const wxColour& color)
{
	m_fontColor = color;
	updateState();
}

void FFButton::SetFontHoverColor(const wxColour& color)
{
	m_fontHoverColor = color;
	updateState();
}

void FFButton::SetFontPressColor(const wxColour& color)
{
	m_fontPressColor = color;
	updateState();
}

void FFButton::SetFontDisableColor(const wxColour& color)
{
	m_fontDisableColor = color;
	updateState();
}

void FFButton::SetFontUniformColor(const wxColour& color)
{
	m_fontColor = color;
	m_fontHoverColor = color;
	m_fontPressColor = color;
	m_fontDisableColor = color;
	updateState();
}

void FFButton::SetBorderColor(const wxColour& color)
{
	m_borderColor = color;
	Update();
}

void FFButton::SetBorderHoverColor(const wxColour& color)
{
	m_borderHoverColor = color;
	Update();
}

void FFButton::SetBorderPressColor(const wxColour& color)
{
	m_borderPressColor = color;
	Update();
}

void FFButton::SetBorderDisableColor(const wxColour& color)
{
	m_borderDisableColor = color;
	Update();
}

void FFButton::SetBGColor(const wxColour& color)
{
	m_bgColor = color;
	Update();
}

void FFButton::SetBGHoverColor(const wxColour& color)
{
	m_bgHoverColor = color;
	Update();
}

void FFButton::SetBGPressColor(const wxColour& color)
{
	m_bgPressColor = color;
	Update();
}

void FFButton::SetBGDisableColor(const wxColour& color)
{
	m_bgDisableColor = color;
	Update();
}

void FFButton::SetBGUniformColor(const wxColour& color)
{
	m_bgColor = color;
	m_bgHoverColor = color;
	m_bgPressColor = color;
	m_bgDisableColor = color;
	Update();
}

void FFButton::OnPaint(wxPaintEvent& event)
{
	wxPaintDC dc(this);
    wxSize size = GetSize();
#ifdef __WXMSW__
    wxMemoryDC memdc;
    wxBitmap   bmp(size.x, size.y);
    memdc.SelectObject(bmp);
    memdc.Blit({0, 0}, size, &dc, {0, 0});
    {
        wxGCDC dc2(memdc);
		dc2.SetFont(GetFont());
        render(dc2);
    }
    memdc.SelectObject(wxNullBitmap);
    dc.DrawBitmap(bmp, 0, 0);
#else
    render(dc);
#endif

	if (!IsEnabled()) {
		dc.SetTextForeground(m_fontDisableColor);
	} else if (m_pressFlag) {
		dc.SetTextForeground(m_fontPressColor);
	} else if (m_hoverFlag) {
		dc.SetTextForeground(m_fontHoverColor);
	} else {
		dc.SetTextForeground(m_fontColor);
	}
	// For Text: Just align-center
	dc.SetFont(GetFont());
	auto text = GetLabel();
    auto textSize = dc.GetMultiLineTextExtent(text);
    auto pt = wxPoint((size.x - textSize.x) / 2, (size.y - textSize.y) / 2);
    dc.DrawText(text, pt);
}

void FFButton::render(wxDC &dc)
{
	wxSize size = GetSize();
	if (!IsEnabled()) {
		dc.SetPen(m_borderDisableColor);
		dc.SetBrush(m_bgDisableColor);
		dc.SetTextForeground(m_fontDisableColor);
	} else if (m_pressFlag) {
		dc.SetPen(m_borderPressColor);
		dc.SetBrush(m_bgPressColor);
		dc.SetTextForeground(m_fontPressColor);
	} else if (m_hoverFlag) {		
		dc.SetPen(m_borderHoverColor);
		dc.SetBrush(m_bgHoverColor);
		dc.SetTextForeground(m_fontHoverColor);
	} else {
		dc.SetPen(m_borderColor);
		dc.SetBrush(m_bgColor);
		dc.SetTextForeground(m_fontColor);
	}
	if (!m_borderFlag) {
		dc.SetPen(*wxTRANSPARENT_PEN);
	}
	if (0 == m_borderRadius) {
		dc.DrawRectangle(0, 0, size.x, size.y);
	} else {
		dc.DrawRoundedRectangle(0, 0, size.x, size.y, m_borderRadius);
	}
	/*dc.SetFont(GetFont());
	auto text = GetLabel();
    auto textSize = dc.GetMultiLineTextExtent(text);
    auto pt = wxPoint((size.x - textSize.x) / 2, (size.y - textSize.y) / 2);
    dc.DrawText(text, pt);*/
}
  
void FFButton::updateState()
{
	if (!IsEnabled()) {
		SetForegroundColour(m_fontDisableColor);
	} else if (m_pressFlag) {
		SetForegroundColour(m_fontPressColor);
	} else if (m_hoverFlag) {
		SetForegroundColour(m_fontHoverColor);
	} else {
		SetForegroundColour(m_fontColor);
	}
	Update();
}
