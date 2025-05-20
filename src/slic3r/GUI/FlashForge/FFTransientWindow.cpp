#include "FFTransientWindow.hpp"
#include <memory>
#include <wx/event.h>
#include <wx/dcclient.h>
#include <wx/graphics.h>
#include "slic3r/GUI/GUI_App.hpp"

namespace Slic3r { namespace GUI {

FFRoundedWindow::FFRoundedWindow(wxWindow *parent)
    : wxPopupWindow(parent, wxBORDER_NONE | wxFRAME_SHAPED)
    , m_radius(FromDIP(6))
{
    SetBackgroundColour(*wxWHITE);
    Bind(wxEVT_SIZE, &FFRoundedWindow::OnSize, this);
    Bind(wxEVT_PAINT, &FFRoundedWindow::OnPaint, this);
}

void FFRoundedWindow::OnSize(wxSizeEvent &evt)
{
    evt.Skip();
    wxEventBlocker evtBlocker(this, wxEVT_SIZE);
    wxGraphicsPath path = wxGraphicsRenderer::GetDefaultRenderer()->CreatePath();
    path.AddRoundedRectangle(0, 0, GetSize().x, GetSize().y, m_radius);
    SetShape(path);
}

void FFRoundedWindow::OnPaint(wxPaintEvent &evt)
{
    wxPaintDC dc(this);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc == nullptr) {
        return;
    }
    gc->SetPen(wxColour("#c1c1c1"));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->DrawRoundedRectangle(0, 0, GetSize().x - 1, GetSize().y - 1, m_radius);
}

FFTransientWindow::FFTransientWindow(wxWindow *parent, wxString titleText)
    : FFRoundedWindow(parent)
    , m_titlePanel(new wxPanel(this))
    , m_titleLbl(new wxStaticText(this, wxID_ANY, titleText, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER))
    , m_closeBmp(create_scaled_bitmap("title_close", this, 12))
    , m_closeHoverBmp(create_scaled_bitmap("title_closeHover", this, 12))
    , m_isHoverClose(false)
    , m_closeStaticBmp(new wxStaticBitmap(this, wxID_ANY, m_closeBmp))
    , m_mainSizer(new wxBoxSizer(wxVERTICAL))
{
    wxColour titleBackgroundColor("#e1e2e6");
    m_titleLbl->SetForegroundColour(wxColour("#333333"));
    m_titleLbl->SetBackgroundColour(titleBackgroundColor);
    m_closeStaticBmp->SetBackgroundColour(titleBackgroundColor);

    wxBoxSizer *titleSizer = new wxBoxSizer(wxHORIZONTAL);
    titleSizer->AddSpacer(FromDIP(12) + m_closeBmp.GetSize().x);
    titleSizer->Add(m_titleLbl, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, FromDIP(8));
    titleSizer->Add(m_closeStaticBmp, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, FromDIP(12));

    m_titlePanel->SetSizer(titleSizer);
    m_titlePanel->SetMinSize(wxSize(-1, FromDIP(38)));
    m_titlePanel->SetBackgroundColour(titleBackgroundColor);

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_titlePanel, 0, wxEXPAND | wxALIGN_TOP);
    sizer->Add(m_mainSizer, 0, wxEXPAND);
    SetSizer(sizer);

    Bind(wxEVT_LEFT_DOWN, &FFTransientWindow::OnLeftDown, this);
    Bind(wxEVT_MOTION, &FFTransientWindow::OnMotion, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &FFTransientWindow::OnMouseCaptureLost, this);
    wxGetApp().Bind(wxEVT_ACTIVATE_APP, &FFTransientWindow::OnActivateApp, this);
}

bool FFTransientWindow::Show(bool show /* = true */)
{
    if (FFRoundedWindow::Show(show)) {
        if (show) {
            CaptureMouse();
        } else {
            SetHoverClose(false);
            ReleaseMouse();
        }
        return true;
    }
    return false;
}

void FFTransientWindow::OnLeftDown(wxMouseEvent &evt)
{
    evt.Skip();
    wxPoint pos = evt.GetPosition();
    if (HitTest(pos) == wxHT_WINDOW_OUTSIDE) {
        Show(false);
        return;
    }
    wxPoint pos1 = m_closeStaticBmp->ScreenToClient(ClientToScreen(pos));
    if (m_closeStaticBmp->HitTest(pos1) == wxHT_WINDOW_INSIDE) {
        Show(false);
    }
}

void FFTransientWindow::OnMotion(wxMouseEvent &evt)
{
    evt.Skip();
    wxPoint pos = evt.GetPosition();
    if (HitTest(pos) == wxHT_WINDOW_OUTSIDE) {
        return;
    }
    wxPoint pos1 = m_closeStaticBmp->ScreenToClient(ClientToScreen(pos));
    SetHoverClose(m_closeStaticBmp->HitTest(pos1) == wxHT_WINDOW_INSIDE);
}

void FFTransientWindow::OnMouseCaptureLost(wxMouseCaptureLostEvent &evt)
{
    evt.Skip();
    SetHoverClose(false);
    FFRoundedWindow::Show(false);
}

void FFTransientWindow::OnActivateApp(wxActivateEvent& event)
{
    event.Skip();
    if (event.GetActive()) {
        return;
    }
    Show(false);
}

void FFTransientWindow::SetHoverClose(bool isHover)
{
    if (m_isHoverClose == isHover) {
        return;
    }
    m_isHoverClose = isHover;
    m_closeStaticBmp->SetBitmap(isHover ? m_closeHoverBmp : m_closeBmp);
    m_closeStaticBmp->Refresh();
    m_closeStaticBmp->Update();
}

}} // namespace Slic3r::GUI
