#include "FFTitleLessDialog.hpp"
#include <memory>
#include <wx/graphics.h>

namespace Slic3r { namespace GUI {

FFTitleLessDialog::FFTitleLessDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxFRAME_SHAPED | wxNO_BORDER)
    , m_radius(FromDIP(6))
    , m_isHoverClose(false)
    , m_isPressClose(false)
    , m_closeBmp(this, "title_less_close", 12)
    , m_closeHoverBmp(this, "title_less_closeHover", 12)
    , m_closePressBmp(this, "title_less_closePress", 12)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &FFTitleLessDialog::onPaint, this);
    Bind(wxEVT_SIZE, &FFTitleLessDialog::onSize, this);
    Bind(wxEVT_CLOSE_WINDOW, &FFTitleLessDialog::onClose, this);
    Bind(wxEVT_LEAVE_WINDOW, &FFTitleLessDialog::onLeave, this);
    Bind(wxEVT_LEFT_DOWN, &FFTitleLessDialog::onLeftDown, this);
    Bind(wxEVT_LEFT_UP, &FFTitleLessDialog::onLeftUp, this);
    Bind(wxEVT_MOTION, &FFTitleLessDialog::onMotion, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &FFTitleLessDialog::onMouseCaptureLost, this);
}

void FFTitleLessDialog::drawBackground(wxBufferedPaintDC &dc, wxGraphicsContext *gc)
{
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(*wxWHITE_BRUSH);
    gc->DrawRectangle(0, 0, GetSize().x, GetSize().y);
}

void FFTitleLessDialog::onPaint(wxPaintEvent &event)
{
    wxBufferedPaintDC dc(this);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc == nullptr) {
        return;
    }
    drawBackground(dc, gc.get());
    gc->SetPen(wxColour("#c1c1c1"));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->DrawRoundedRectangle(0, 0, GetSize().x - 1, GetSize().y - 1, m_radius);

    wxBitmap *bmp = nullptr;
    if (m_isPressClose) {
        bmp = &m_closePressBmp.bmp();
    } else if (m_isHoverClose) {
        bmp = &m_closeHoverBmp.bmp();
    } else {
        bmp = &m_closeBmp.bmp();
    }
    gc->DrawBitmap(*bmp, m_closeRect.x, m_closeRect.y, m_closeRect.width, m_closeRect.height);
}

void FFTitleLessDialog::onSize(wxSizeEvent &event)
{
    wxEventBlocker blocker(this, wxEVT_SIZE);
    wxGraphicsPath path = wxGraphicsRenderer::GetDefaultRenderer()->CreatePath();
    path.AddRoundedRectangle(0, 0, GetSize().x, GetSize().y, m_radius);
    SetShape(path);

    int margin = FromDIP(10);
    m_closeRect.x = GetSize().x - margin - m_closeBmp.GetBmpSize().x;
    m_closeRect.y = margin;
    m_closeRect.width = m_closeBmp.GetBmpSize().x;
    m_closeRect.height = m_closeBmp.GetBmpSize().y;
}

void FFTitleLessDialog::onClose(wxCloseEvent &event)
{
    event.Skip();
    EndModal(wxID_CANCEL);
}

void FFTitleLessDialog::onLeave(wxMouseEvent &event)
{
    event.Skip();
    if (m_isHoverClose) {
        m_isHoverClose = false;
        Update();
        Refresh();
    }
}

void FFTitleLessDialog::onLeftDown(wxMouseEvent &event)
{
    if (!m_closeRect.Contains(event.GetPosition())) {
        event.Skip();
        return;
    }
    m_isPressClose = true;
    Update();
    Refresh();
    if (!HasCapture()) {
        CaptureMouse();
    }
}

void FFTitleLessDialog::onLeftUp(wxMouseEvent &event)
{
    if (!m_isPressClose) {
        event.Skip();
        return;
    }
    if (m_closeRect.Contains(event.GetPosition())) {
        Close();
    }
    m_isHoverClose = false;
    m_isPressClose = false;
    Update();
    Refresh();
    if (HasCapture()) {
        ReleaseMouse();
    }
}

void FFTitleLessDialog::onMotion(wxMouseEvent &event)
{
    event.Skip();
    bool isHoverClose = m_closeRect.Contains(event.GetPosition());
    if (m_isHoverClose != isHoverClose) {
        m_isHoverClose = isHoverClose;
        Update();
        Refresh();
    }
}

void FFTitleLessDialog::onMouseCaptureLost(wxMouseCaptureLostEvent &event)
{
    m_isHoverClose = false;
    m_isPressClose = false;
    Update();
    Refresh();
}

}} // Slic3r::GUI
