#include "FFButton.hpp"
#include <wx/dcgraph.h>
#include "slic3r/GUI/wxExtensions.hpp"


FFButton::FFButton(wxWindow* parent, wxWindowID id/*= wxID_ANY*/, const wxString& label/*= ""*/,
	int borderRadius/*=4*/, bool borderFlag/* = true*/)
	: wxWindow(parent, id, wxDefaultPosition, wxDefaultSize, wxNO_BORDER)
	, m_hoverFlag(false)
	, m_pressFlag(false)
	, m_borderFlag(borderFlag)
    , m_enable(true)
	, m_borderRadius(borderRadius)
	, m_fontColor("#333333")
	, m_fontHoverColor("#65A79E")
	, m_fontPressColor("#419488")
	, m_fontDisableColor("#333333")
	, m_borderColor("#333333")
	, m_borderHoverColor("#65A79E")
	, m_borderPressColor("#419488")
	, m_borderDisableColor("#dddddd")
	, m_bgColor("#ffffff")
	, m_bgHoverColor("#ffffff")
	, m_bgPressColor("#ffffff")
	, m_bgDisableColor("#dddddd")
{
	if (parent) {
		SetBackgroundColour(parent->GetBackgroundColour());	
	}
	SetLabel(label);
	Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent& e) { m_hoverFlag = true; Refresh(); e.Skip(); });
	Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& e) { m_hoverFlag = false; m_pressFlag = false; Refresh(); e.Skip(); });
	Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) { m_pressFlag = true; Refresh(); e.Skip(); });
	Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) { m_pressFlag = false; Refresh(); sendEvent(); e.Skip(); });
	Bind(wxEVT_PAINT, &FFButton::OnPaint, this);
	Bind(wxEVT_ERASE_BACKGROUND, [=](auto& e) {
		e.Skip();
	});
	Layout();
	Fit();
	//updateState();
}

bool FFButton::Enable(bool enable/* = true*/)
{
	bool ret = wxWindow::Enable(enable);
	if (ret) {
		Refresh();
	}
	return ret;
}


void FFButton::SetEnable(bool enable) 
{
	m_enable = enable; 
	Refresh();
}

void FFButton::SetLabel(const wxString & label)
{
	wxWindow::SetLabel(label);
	wxScreenDC dc;
	//font = wx.Font(14, wx.DEFAULT, wx.NORMAL, wx.NORMAL)
	//wxPaintDC dc(this);
	dc.SetFont(GetFont());
	auto textSize = dc.GetTextExtent(label);
	SetMinSize(textSize+wxSize(40, 14));
	Refresh();
}

void FFButton::SetFontColor(const wxColour& color)
{
	m_fontColor = color;
	Update();
	//updateState();
}

void FFButton::SetFontHoverColor(const wxColour& color)
{
	m_fontHoverColor = color;
	Update();
}

void FFButton::SetFontPressColor(const wxColour& color)
{
	m_fontPressColor = color;
	Update();
}

void FFButton::SetFontDisableColor(const wxColour& color)
{
	m_fontDisableColor = color;
	Update();
}

void FFButton::SetFontUniformColor(const wxColour& color)
{
	m_fontColor = color;
	m_fontHoverColor = color;
	m_fontPressColor = color;
	m_fontDisableColor = color;
	Update();
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
    wxBitmap bmp(size.x, size.y);
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
	wxString text = GetLabel();
	if (!text.IsEmpty()) {
        if (!IsEnabled() || !m_enable) {
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
		auto textSize = dc.GetMultiLineTextExtent(text);
		auto pt = wxPoint((size.x - textSize.x) / 2, (size.y - textSize.y) / 2);
		dc.DrawText(text, pt);
	}
	event.Skip();
}

void FFButton::render(wxDC &dc)
{
	wxSize size = GetSize();
    if (!IsEnabled() || !m_enable) {
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
    if (!IsEnabled() || !m_enable) {
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

void FFButton::sendEvent()
{
	wxCommandEvent event(wxEVT_BUTTON);
	event.SetEventObject(this);
	event.SetId(GetId());
	wxPostEvent(this, event);
}


FFPushButton::FFPushButton(wxWindow *parent,wxWindowID      id,const wxString &normalIcon,const wxString &hoverIcon,const wxString &pressIcon,const wxString &disableIcon,const int iconSize)
    : wxButton(parent, id, "", wxPoint(10, 10), wxSize(25, 25), wxNO_BORDER)
    , m_normalIcon(normalIcon)
    , m_hoverIcon(hoverIcon)
    , m_pressIcon(pressIcon)
    , m_disableIcon(disableIcon)
{
    //SetBitmap(wxBitmap(normalIcon));
    m_normalBitmap  = create_scaled_bitmap(normalIcon.ToStdString(), this, iconSize);
    m_hoverBitmap   = create_scaled_bitmap(hoverIcon.ToStdString(), this, iconSize);
    m_pressBitmap   = create_scaled_bitmap(pressIcon.ToStdString(), this, iconSize);
    m_disableBitmap = create_scaled_bitmap(disableIcon.ToStdString(), this, iconSize);
    Bind(wxEVT_PAINT, &FFPushButton::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &FFPushButton::OnMousePress, this);
    Bind(wxEVT_LEFT_UP, &FFPushButton::OnMouseRelease, this);
    Bind(wxEVT_ENTER_WINDOW, &FFPushButton::OnMouseEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &FFPushButton::OnMouseLeave, this);
}

void FFPushButton::OnPaint(wxPaintEvent &event) 
{
    wxPaintDC dc(this);
    if (IsEnabled()) {
        if (m_isPressed) {
            dc.DrawBitmap(m_pressBitmap, 0, 0, true);
        } else if (m_isHover) {
            dc.DrawBitmap(m_hoverBitmap, 0, 0, true);
        } else {
            dc.DrawBitmap(m_normalBitmap, 0, 0, true);
        }
    } else {
        dc.DrawBitmap(m_disableBitmap, 0, 0, true);
    }
}
