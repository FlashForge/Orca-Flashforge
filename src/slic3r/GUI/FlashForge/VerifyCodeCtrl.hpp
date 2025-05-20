#ifndef slic3r_GUI_VerifyCodeCtrl_hpp_
#define slic3r_GUI_VerifyCodeCtrl_hpp_

#include <wx/textctrl.h>
#include "slic3r/GUI/wxExtensions.hpp"

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/dcclient.h>

namespace Slic3r { 
namespace GUI {

class VerifyCodeCtrl :  public wxPanel
{
public:
    VerifyCodeCtrl(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& value = wxEmptyString, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxPanelNameStr) ;
    virtual ~VerifyCodeCtrl();

    void SetRadius(int r);
    wxString GetValue() {return m_text_ctrl->GetValue();}
    void     ClearTxt() { m_text_ctrl->Clear();};

private:
    void OnPaint(wxPaintEvent& event);
    void render(wxDC& dc);

private:
    wxTextCtrl* m_text_ctrl{nullptr};
    wxBitmap    m_icon;
    int         m_radius;

    wxPanel*    m_panel_page {nullptr};
};

#endif // slic3r_GUI_VerifyCodeCtrl_hpp_

} // namespace GUI
} // namespace Slic3r
