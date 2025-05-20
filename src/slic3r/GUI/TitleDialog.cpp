#include "TitleDialog.hpp"
#include <wx/event.h>
#include <wx/stattext.h>
#include <wx/graphics.h>
#include <wx/dcgraph.h>
#include "wxExtensions.hpp"
#include "libslic3r/Utils.hpp"

namespace Slic3r::GUI {

TitleBar::TitleBar(wxWindow *parent, const wxString& title, const wxColour& color, int borderRadius/*= 6*/)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    , m_dragging(false)
    , m_borderRadius(borderRadius)
    , m_bgColor(color)
    , m_title(title)
{
    m_titleLbl = new wxStaticText(this, wxID_ANY, m_title, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    wxFont font = m_titleLbl->GetFont();
    font.SetWeight(wxFONTWEIGHT_BOLD);
    m_titleLbl->SetFont(font);
    m_titleLbl->Bind(wxEVT_LEFT_DOWN, &TitleBar::OnMouseLeftDown, this);
    m_titleLbl->SetBackgroundColour(m_bgColor);

    m_closeBtn = new wxBitmapButton(this, -1, create_scaled_bitmap("title_close", this, 12), wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    m_closeBtn->SetBitmapHover(create_scaled_bitmap("title_closeHover", this, 12));
    m_closeBtn->SetBitmapPressed(create_scaled_bitmap("title_closePress", this, 12));
    m_closeBtn->SetBackgroundColour(color);
    m_closeBtn->Bind(wxEVT_LEFT_DOWN, &TitleBar::OnCloseClicked, this);
    
    wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);
    mainSizer->Add(m_titleLbl, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 8);
    mainSizer->Add(m_closeBtn, 0, wxRIGHT | wxALIGN_CENTER, 8);
    mainSizer->AddSpacer(4);
    SetSizer(mainSizer);
    Layout();
    
    Bind(wxEVT_PAINT, &TitleBar::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &TitleBar::OnMouseLeftDown, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &TitleBar::OnMouseCaptureLost, this);
}

wxSize TitleBar::DoGetBestClientSize() const
{
    return wxSize(-1, FromDIP(38));
}

void TitleBar::SetTitle(const wxString& title)
{
    m_titleLbl->SetLabel(title);
}

void TitleBar::OnPaint(wxPaintEvent& event)
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
        DoRender(dc2);
    }
    memdc.SelectObject(wxNullBitmap);
    dc.DrawBitmap(bmp, 0, 0);
#else
    DoRender(dc);
#endif
}

void TitleBar::DoRender(wxDC &dc)
{
    wxSize sz = GetSize();
	dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxColour("#c1c1c1"));
    dc.DrawRectangle(0, 0, sz.x, sz.y);

    dc.SetBrush(m_bgColor);
    dc.DrawRoundedRectangle(0, 0, sz.x, sz.y / 2, m_borderRadius);
    dc.DrawRectangle(0, m_borderRadius+3, sz.x, sz.y);
}

void TitleBar::OnMouseLeftDown(wxMouseEvent &event)
{
    if (!m_dragging) {
        Bind(wxEVT_LEFT_UP, &TitleBar::OnMouseLeftUp, this);
        Bind(wxEVT_MOTION, &TitleBar::OnMouseMotion, this);
        m_dragging = true;

        wxPoint clientStart = event.GetPosition();
        if (event.GetId() == m_titleLbl->GetId()) {
            clientStart += m_titleLbl->GetPosition();
        }
        m_dragStartMouse = ClientToScreen(clientStart);
        m_dragStartWindow = GetParent()->GetPosition();
        CaptureMouse();
        BOOST_LOG_TRIVIAL(info) << "TitleBar::CaptureMouse";
        flush_logs();
    }
}
void TitleBar::OnMouseLeftUp(wxMouseEvent &event)
{
    FinishDrag();
}

void TitleBar::OnMouseMotion(wxMouseEvent &event)
{
    wxPoint curClientPnt = event.GetPosition();
    wxPoint curScreenPnt = ClientToScreen(curClientPnt);
    wxPoint movementVector = curScreenPnt - m_dragStartMouse;
    GetParent()->SetPosition(m_dragStartWindow + movementVector);
}

void TitleBar::OnMouseCaptureLost(wxMouseCaptureLostEvent& event)
{
    FinishDrag();
}

void TitleBar::OnCloseClicked(wxMouseEvent& event) 
{
    event.Skip();
    if (GetParent()) {
        wxDialog* dlg = static_cast<wxDialog*>(GetParent());
        if (dlg && dlg->IsModal()) {
            dlg->EndModal(wxID_CANCEL);
        } else {
            GetParent()->Close();
        }
        BOOST_LOG_TRIVIAL(info) << "TitleBar::OnClose";
        flush_logs();
    }
}

void TitleBar::FinishDrag()
{
    if (m_dragging) {
        Unbind(wxEVT_LEFT_UP, &TitleBar::OnMouseLeftUp, this);
        Unbind(wxEVT_MOTION, &TitleBar::OnMouseMotion, this);
        m_dragging = false;
    }
    if (HasCapture()) {
        ReleaseMouse();
        BOOST_LOG_TRIVIAL(info) << "TitleBar::ReleaseMouse";
        flush_logs();
    }
}

TitleDialog::TitleDialog(wxWindow* parent, const wxString& title, int borderRadius/*=6*/, const wxSize &size/*=wxDefaultSize*/)
    : DPIDialog(parent, wxID_ANY, "", wxDefaultPosition, size, wxFRAME_SHAPED | wxNO_BORDER)
    , m_borderRadius(borderRadius)
    , m_titleBar(new TitleBar(this, title, "#E1E2E6", borderRadius))
    , m_mainSizer(new wxBoxSizer(wxVERTICAL))
{
    SetBackgroundColour(*wxWHITE);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddSpacer(m_shadow_width);
    sizer->Add(m_titleBar, 0, wxEXPAND | wxALIGN_TOP | wxLEFT | wxRIGHT, m_shadow_width);
    sizer->Add(m_mainSizer, 1, wxEXPAND | wxLEFT | wxRIGHT, m_shadow_width);
    sizer->AddSpacer(m_borderRadius);
    SetSizer(sizer);
    Layout();

    Bind(wxEVT_PAINT, &TitleDialog::OnPaint, this);
    Bind(wxEVT_SIZE, &TitleDialog::OnSize, this);
}

void TitleDialog::SetTitleBackgroundColor(const wxColour& color)
{
    m_titleBar->SetBackgroundColour(color);
}

void TitleDialog::SetSize(const wxSize& size)
{
    wxSize _size = size;
    _size.y += m_titleBar->GetSize().y;
    DPIDialog::SetSize(_size);
}

wxSize TitleDialog::GetSize() const
{
    wxSize sz = DPIDialog::GetSize();
    sz.y -= m_titleBar->GetSize().y;
    return sz;
}

void TitleDialog::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    wxSize size = DPIDialog::GetSize();
#ifdef __WXMSW__
    wxMemoryDC memdc;
    wxBitmap bmp(size.x, size.y);
    memdc.SelectObject(bmp);
    memdc.Blit({0, 0}, size, &dc, {0, 0});
    {
        wxGCDC dc2(memdc);
        DoRender(dc2);
    }
    memdc.SelectObject(wxNullBitmap);
    dc.DrawBitmap(bmp, 0, 0);
#else
    DoRender(dc);
#endif
}

void TitleDialog::DoRender(wxDC &dc)
{
    wxSize sz = DPIDialog::GetSize();
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxColour("#c1c1c1"));
    dc.DrawRectangle(0, 0, sz.x, sz.y);
    dc.SetBrush(wxColour(255, 255, 255));
    dc.DrawRoundedRectangle(m_shadow_width, m_shadow_width, sz.x - 2 * m_shadow_width, sz.y - 2 * m_shadow_width, m_borderRadius);
}

void TitleDialog::OnSize(wxSizeEvent& event)
{
    wxEventBlocker blocker(this, wxEVT_SIZE);
    wxGraphicsPath path = wxGraphicsRenderer::GetDefaultRenderer()->CreatePath();
    wxSize size = DPIDialog::GetSize();
    path.AddRoundedRectangle(0, 0, size.x, size.y, m_borderRadius+1);
    SetShape(path);
}

wxBoxSizer* TitleDialog::MainSizer()
{
    return m_mainSizer;
}

} // end namespace
