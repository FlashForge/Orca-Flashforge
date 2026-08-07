#include "FFPopupWindow.hpp"
#include <wx/app.h> 
#include "slic3r/GUI/GUI_App.hpp"


FFPopupWindow::FFPopupWindow(wxWindow* parent)
    : wxPopupWindow(parent, wxBORDER_NONE | wxFRAME_SHAPED)
{
    //Reparent(parent);
    //SetWindowStyle(wxBORDER_NONE | wxFRAME_SHAPED);
    Bind(wxEVT_PAINT, &FFPopupWindow::onPaint, this);
    Bind(wxEVT_LEFT_DOWN, &FFPopupWindow::onLeftDown, this);
    Bind(wxEVT_LEFT_UP, &FFPopupWindow::onLeftUp, this);
    Bind(wxEVT_MOTION, &FFPopupWindow::onMotion, this);
    //Bind(wxEVT_ACTIVATE, &FFPopupWindow::onActivate, this);
}

FFPopupWindow::~FFPopupWindow()
{
    Dismiss();
}

void FFPopupWindow::Popup(wxWindow* focus/*=nullptr*/)
{
    if (focus) {
        wxPoint pos = focus->ClientToScreen(wxPoint(0, focus->GetSize().y + 2));
        Move(pos);
    }
    Show();
}

bool FFPopupWindow::Show(bool show/* = true*/)
{
    if (show) {
        bool changed = wxPopupWindow::Show(true);
        if (!m_mouseCaptured) {
            CaptureMouse();
            m_mouseCaptured = true;
            Bind(wxEVT_MOUSE_CAPTURE_LOST, &FFPopupWindow::onCaptureMouseLost, this);
            Slic3r::GUI::wxGetApp().Bind(wxEVT_ACTIVATE_APP, &FFPopupWindow::onActivateApp, this);
        }
        return changed;
    } else {
        // Always drop our mouse capture when hiding -- even if the window was
        // already hidden. Otherwise a stale pointer is left on the global
        // wxMouseCapture::stack, and a later wxDialog::ShowModal() that walks
        // that stack (via NotifyCaptureLost) dereferences freed memory.
        releaseCapture(/*callReleaseMouse=*/true);
        bool changed = wxPopupWindow::Show(false);
        if (changed)
            OnDismiss();
        return changed;
    }
}

void FFPopupWindow::releaseCapture(bool callReleaseMouse)
{
    if (!m_mouseCaptured)
        return;
    m_mouseCaptured = false;
    Unbind(wxEVT_MOUSE_CAPTURE_LOST, &FFPopupWindow::onCaptureMouseLost, this);
    Slic3r::GUI::wxGetApp().Unbind(wxEVT_ACTIVATE_APP, &FFPopupWindow::onActivateApp, this);
    if (callReleaseMouse && HasCapture())
        ReleaseMouse();
}

void FFPopupWindow::Dismiss()
{
    Hide();
}

void FFPopupWindow::OnDismiss()
{
}

void FFPopupWindow::onPaint(wxPaintEvent& event)
{
    auto sz = GetSize();
    wxPaintDC dc(this);
    dc.SetPen(*wxTRANSPARENT_PEN);
    //dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetBrush(GetBackgroundColour());
    dc.DrawRectangle(0, 0, sz.x, sz.y);
}

void FFPopupWindow::onShow(wxShowEvent& event)
{
    if (event.IsShown()) {
        CaptureMouse();
    } else {
        ReleaseMouse();
        OnDismiss();
    }
}

void FFPopupWindow::onActivateApp(wxActivateEvent &event)
{
    if (!event.GetActive()) {
        Dismiss();
    }
    event.Skip();
}

void FFPopupWindow::onCaptureMouseLost(wxMouseCaptureLostEvent& event)
{
    // The system already revoked the capture, so don't call ReleaseMouse() --
    // just clear our bookkeeping and unbind, then dismiss.
    releaseCapture(/*callReleaseMouse=*/false);
    Dismiss();
    event.Skip();
}

void FFPopupWindow::onLeftDown(wxMouseEvent &event)
{
    wxPoint pnt = event.GetPosition();
    if (wxHT_WINDOW_OUTSIDE == HitTest(pnt)) {
        Dismiss();
    } else {
        ProcessLeftDown(pnt);
        m_leftPressed = true;
    }
    event.Skip();
    
}

void FFPopupWindow::onLeftUp(wxMouseEvent &event)
{
    wxPoint pnt = event.GetPosition();
    if (m_leftPressed) {
        m_leftPressed = false;
        ProcessLeftUp(pnt);
    }
    event.Skip();
}

void FFPopupWindow::onMotion(wxMouseEvent &event)
{
    ProcessMotion(event.GetPosition());
    event.Skip();
}
