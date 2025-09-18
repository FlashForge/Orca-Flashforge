#ifndef slic3r_GUI_PasswordCtrl_hpp_
#define slic3r_GUI_PasswordCtrl_hpp_

#include <wx/textctrl.h>
#include "slic3r/GUI/wxExtensions.hpp"

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/dcclient.h>

namespace Slic3r { 
namespace GUI {

class PasswordCtrl :  public wxPanel
{
public:
    PasswordCtrl(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& value = wxEmptyString, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxPanelNameStr) ;
    virtual ~PasswordCtrl();

    void SetRadius(int r);
    wxString GetValue(); 
    void RefreshEyePicPosition();
    void ShowEncrypt();
    void ClearTxt();

private:
    void OnPaint(wxPaintEvent& event);
    void render(wxDC& dc);
    void OnShowPasswordButtonClicked(wxMouseEvent& event);

private:
    wxTextCtrl* m_text_ctrl {nullptr};
    wxTextCtrl* m_plain_text_ctrl {nullptr};
    wxBitmap    m_icon;
    wxBitmap    m_eye_off_bitmap;
    wxBitmap    m_eye_on_bitmap;
    wxStaticBitmap* m_eye_show_icon {nullptr};
    int         m_radius;
    bool        m_encrypt = true;
    wxPoint     m_eye_pic_position;

    wxPanel*    m_panel_page {nullptr};
};

#endif // slic3r_GUI_PasswordCtrl_hpp_

} // namespace GUI
} // namespace Slic3r
