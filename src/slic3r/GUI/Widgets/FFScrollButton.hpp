#ifndef slic3r_GUI_FFScrollButton_hpp_
#define slic3r_GUI_FFScrollButton_hpp_

#include <wx/wx.h>
#include <wx/timer.h>

#include "FFButton.hpp"


class FFScrollButton : public FFButton
{
public:
    FFScrollButton(
        wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& label = "", int borderRadius = 4, bool borderFlag = true);
    ~FFScrollButton();

    void SetLabel(const wxString& label) override;

private:
    void OnTimer(wxTimerEvent& event);
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);

    void CheckIfScrollNeeded();
    int  GetTextWidth();

private:
    int      m_offset = 100;
    int      m_extra_offset = 20;
    bool     m_should_scroll;
    int      m_speed = 2;
    wxTimer* m_timer;
};

#endif