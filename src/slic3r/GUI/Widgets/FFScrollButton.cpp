#include "FFScrollButton.hpp"

#include <wx/dcgraph.h>

FFScrollButton::FFScrollButton(wxWindow* parent, wxWindowID id/* = wxID_ANY*/, const wxString& label/* = ""*/, int borderRadius/* = 4*/, bool borderFlag/* = true*/)
    : FFButton(parent, id, label, borderRadius, borderFlag)
{
    m_timer = new wxTimer(this, wxID_ANY);
    m_timer->Start(25);

    Bind(wxEVT_TIMER, &FFScrollButton::OnTimer, this);
    Bind(wxEVT_PAINT, &FFScrollButton::OnPaint, this);
    Bind(wxEVT_SIZE, &FFScrollButton::OnSize, this);
}

FFScrollButton::~FFScrollButton()
{
    if (m_timer != NULL) {
        m_timer->Stop();
        delete m_timer;
        m_timer = NULL;
    }
}

void FFScrollButton::SetLabel(const wxString& label)
{
    m_text = label;
    CheckIfScrollNeeded();

    FFButton::SetLabel(label);
}

void FFScrollButton::OnTimer(wxTimerEvent& event)
{
    if (!m_should_scroll)
        return;

    m_offset -= m_speed;
    if (m_offset < -GetTextWidth() - m_extra_offset)
        m_offset = 0;

    Refresh();
}

void FFScrollButton::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    wxSize    size = GetSize();
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
        if (m_should_scroll) {
            int textWidth = GetTextWidth();
            int    y    = (size.GetHeight() - dc.GetCharHeight()) / 2;
            dc.DrawText(m_text, wxPoint(m_offset, y));
            dc.DrawText(m_text, wxPoint(textWidth + m_offset + m_extra_offset, y));
        } else {
            auto   textSize  = dc.GetMultiLineTextExtent(text);
            int    x         = (size.GetWidth() - textSize.x) / 2;
            int    y         = (size.GetHeight() - dc.GetCharHeight()) / 2;
            dc.DrawText(m_text, wxPoint(x, y));
        }
    }
    //event.Skip();
}

void FFScrollButton::OnSize(wxSizeEvent& event)
{
    CheckIfScrollNeeded();
    event.Skip();
}

void FFScrollButton::CheckIfScrollNeeded()
{
    int textWidth  = GetTextWidth();
    int panelWidth = GetClientSize().GetWidth();

    m_should_scroll = (textWidth > panelWidth);

    if (!m_should_scroll)
        m_offset = 0;

    Refresh();
}

int  FFScrollButton::GetTextWidth()
{
    wxClientDC dc(this);
    dc.SetFont(GetFont());
    return dc.GetTextExtent(m_text).GetWidth();
}


