#ifndef slic3r_GUI_UserNameCtrl_hpp_
#define slic3r_GUI_UserNameCtrl_hpp_

#include <wx/textctrl.h>
#include "slic3r/GUI/wxExtensions.hpp"

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/dcclient.h>

namespace Slic3r { 
namespace GUI {

class UserNameCtrl :  public wxPanel
{
public:
    UserNameCtrl(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& value = wxEmptyString, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0/*wxTAB_TRAVERSAL *//*wxNO_BORDER*/, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxPanelNameStr) ;
    virtual ~UserNameCtrl();

    void SetRadius(int r);
    wxString GetValue() {return m_text_ctrl->GetValue();}
    void  SetTextHint(int value);
    void  ClearTxt() { m_text_ctrl->Clear(); };

private:
    void OnPaint(wxPaintEvent& event);
    void render(wxDC& dc);

private:
    wxTextCtrl* m_text_ctrl{nullptr};
    wxBitmap    m_icon;
    int         m_radius;

    wxPanel*    m_panel_page {nullptr};
};

#endif // slic3r_GUI_UserNameCtrl_hpp_

} // namespace GUI
} // namespace Slic3r
