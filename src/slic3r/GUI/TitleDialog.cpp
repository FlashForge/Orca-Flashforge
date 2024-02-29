#include "TitleDialog.hpp"
#include <wx/stattext.h>
#include "wx/graphics.h"
#include "wxExtensions.hpp"


namespace Slic3r::GUI {

TitleBar::TitleBar(wxWindow *parent, const wxString& title, const wxColour& color, int borderRadius/*= 6*/)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    , m_dragging(false)
    , m_borderRadius(borderRadius)
    , m_bgColor(color)
    , m_title(title)
{
    SetBackgroundColour(m_bgColor);
    m_titleLbl = new wxStaticText(this, wxID_ANY, m_title, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    m_titleLbl->Bind(wxEVT_LEFT_DOWN, &TitleBar::OnMouseLeftDown, this);
    m_titleLbl->SetBackgroundColour(m_bgColor/*wxColour("#ff0000")*/);

    m_closeBtn = new wxBitmapButton(this, -1, create_scaled_bitmap("title_close", this, 12), wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    m_closeBtn->SetBitmapHover(create_scaled_bitmap("title_closeHover", this, 12));
    m_closeBtn->SetBitmapPressed(create_scaled_bitmap("title_closePress", this, 12));
    m_closeBtn->SetBackgroundColour(color);
    //m_closeBtn = new ImageButton(this, "title_close", "title_closeHover", "title_closePress");

    wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);
    //mainSizer->AddStretchSpacer(1);
    mainSizer->Add(m_titleLbl, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 8);
    //mainSizer->AddStretchSpacer(1);
    mainSizer->Add(m_closeBtn, 0, wxRIGHT | wxALIGN_CENTER, 8);
    SetSizer(mainSizer);
    Layout();
    
    Bind(wxEVT_PAINT, &TitleBar::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &TitleBar::OnMouseLeftDown, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &TitleBar::OnMouseCaptureLost, this);
    Bind(wxEVT_BUTTON, &TitleBar::OnClose, this);
}

wxSize TitleBar::DoGetBestClientSize() const
{
    return wxSize(-1, 48);
}

//void TitleBar::SetBackgroundColor(const wxColour& color)
//{
//    SetBackgroundColor
//}

void TitleBar::SetTitle(const wxString& title)
{
    m_titleLbl->SetLabel(title);
}

void TitleBar::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    dc.SetBrush(m_bgColor);
    dc.SetPen(*wxTRANSPARENT_PEN);
    wxSize size = GetSize();
    //dc.DrawRoundedRectangle(0, 0, size.x, size.y, m_borderRadius);
    dc.DrawRectangle(0, size.y / 2, size.x, size.y);
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

void TitleBar::OnClose(wxCommandEvent &event)
{
    if (GetParent()) {
        GetParent()->Close();
    }
    event.Skip();
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
    }
}


// TitleDialog
TitleDialog::TitleDialog(wxWindow* parent, const wxString& title, int borderRadius/*=6*/, const wxSize &size/*=wxDefaultSize*/)
    : DPIDialog(parent, wxID_ANY, "", wxDefaultPosition, size, wxFRAME_SHAPED | wxNO_BORDER)
    , m_borderRadius(borderRadius)
    , m_titleBar(new TitleBar(this, title, "#E1E2E6", borderRadius))
    , m_mainSizer(new wxBoxSizer(wxVERTICAL))
{
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_titleBar, 0, wxEXPAND | wxALIGN_TOP);
    sizer->Add(m_mainSizer, 1, wxEXPAND);
    SetSizer(sizer);
    Layout();

    Bind(wxEVT_PAINT, &TitleDialog::OnPaint, this);
    CallAfter([this](){ Bind(wxEVT_SIZE, &TitleDialog::OnSize, this); });
}

void TitleDialog::SetTitleBackgroundColor(const wxColour& color)
{
    m_titleBar->SetBackgroundColour(color);
}

wxBoxSizer* TitleDialog::MainSizer()
{
    return m_mainSizer;
}

void TitleDialog::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    dc.SetBrush(wxColour(255, 255, 255));
    dc.SetPen(*wxTRANSPARENT_PEN);
    //dc.DrawRoundedRectangle(, m_borderRadius);
    dc.DrawRectangle(GetClientSize());
}

void TitleDialog::OnSize(wxSizeEvent& event)
{
    wxGraphicsPath path = wxGraphicsRenderer::GetDefaultRenderer()->CreatePath();
    wxSize size = event.GetSize();
    path.AddRoundedRectangle(0, 0, size.x, size.y, m_borderRadius);
    SetShape(path);
}

} // end namespace