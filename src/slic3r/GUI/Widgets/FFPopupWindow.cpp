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
    if (wxPopupWindow::Show(show)) {
        if (show) {
            CaptureMouse();
            Bind(wxEVT_MOUSE_CAPTURE_LOST, &FFPopupWindow::onCaptureMouseLost, this);
            Slic3r::GUI::wxGetApp().Bind(wxEVT_ACTIVATE_APP, &FFPopupWindow::onActivateApp, this);
        } else {
            Unbind(wxEVT_MOUSE_CAPTURE_LOST, &FFPopupWindow::onCaptureMouseLost, this);
            Slic3r::GUI::wxGetApp().Unbind(wxEVT_ACTIVATE_APP, &FFPopupWindow::onActivateApp, this);
            ReleaseMouse();
            OnDismiss();
        }
        return true;
    }
    return false;
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
