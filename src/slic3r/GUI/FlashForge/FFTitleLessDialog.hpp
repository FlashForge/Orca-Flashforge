#ifndef slic3r_GUI_FFTitleLessDialog_hpp_
#define slic3r_GUI_FFTitleLessDialog_hpp_

#include <wx/dcbuffer.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include "slic3r/GUI/wxExtensions.hpp"

namespace Slic3r { namespace GUI {

class FFTitleLessDialog : public wxDialog
{
public:
    FFTitleLessDialog(wxWindow *parent);

    virtual void drawBackground(wxBufferedPaintDC &dc, wxGraphicsContext *gc);

private:
    void onPaint(wxPaintEvent &event);

    void onSize(wxSizeEvent &event);

    void onClose(wxCloseEvent &event);

    void onLeave(wxMouseEvent &event);

    void onLeftDown(wxMouseEvent &event);

    void onLeftUp(wxMouseEvent &event);

    void onMotion(wxMouseEvent &event);

    void onMouseCaptureLost(wxMouseCaptureLostEvent &event);

private:
    int    m_radius;
    bool   m_isHoverClose;
    bool   m_isPressClose;
    wxRect m_closeRect;
    ScalableBitmap m_closeBmp;
    ScalableBitmap m_closeHoverBmp;
    ScalableBitmap m_closePressBmp;
};

}} // Slic3r::GUI

#endif
