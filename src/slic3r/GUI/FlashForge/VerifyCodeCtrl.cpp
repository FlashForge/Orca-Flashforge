#include "VerifyCodeCtrl.hpp"

#include "slic3r/GUI/Widgets/TextCtrl.h"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/I18N.hpp"

#include <wx/dcgraph.h>

namespace Slic3r {
namespace GUI {

VerifyCodeCtrl::VerifyCodeCtrl(wxWindow* parent, wxWindowID id/* = wxID_ANY*/, const wxString& value/* = wxEmptyString*/, const wxPoint& pos /*= wxDefaultPosition*/, const wxSize& size/* = wxDefaultSize*/, long style/* = 0*/, const wxValidator& validator /*= wxDefaultValidator*/, const wxString& name /*= wxPanelNameStr*/) 
    : wxPanel(parent, id,pos,size)
    , m_radius(8)
{
    wxBoxSizer* body_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_panel_page = new wxPanel(this, wxID_ANY, wxDefaultPosition,wxSize(FromDIP(190), FromDIP(20)),wxBORDER_NONE);

    m_text_ctrl = new wxTextCtrl(m_panel_page, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_LEFT | wxBORDER_NONE);
    m_text_ctrl->SetHint(_L("Code"));
    m_text_ctrl->SetMinSize(wxSize(FromDIP(110),FromDIP(20)));
    m_icon = create_scaled_bitmap("login-verify", this, 18);

    body_sizer->Add(new wxStaticBitmap(m_panel_page, wxID_ANY, m_icon), 0, wxALL, 0);
    body_sizer->AddSpacer(FromDIP(9));
    body_sizer->Add(m_text_ctrl, 0,  wxALIGN_CENTER);

    m_panel_page->SetSizer(body_sizer);
    m_panel_page->Layout();
    body_sizer->Fit(m_panel_page);

    Bind(wxEVT_PAINT, &VerifyCodeCtrl::OnPaint, this);
    this->SetMinSize(wxSize(FromDIP(170), FromDIP(42)));
}

VerifyCodeCtrl::~VerifyCodeCtrl()
{
    if(m_text_ctrl){
        delete m_text_ctrl;
        m_text_ctrl = nullptr;
    }
    if(m_panel_page){
        delete m_panel_page;
        m_panel_page = nullptr;
    }
}

void VerifyCodeCtrl::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    render(dc);
}

void VerifyCodeCtrl::render(wxDC &dc)
{
    wxSize size = GetSize();

    dc.SetPen(wxPen(wxColour(153,153,153), 1));
    dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
#ifdef __APPLE__
    dc.DrawRoundedRectangle(1, 1, size.x-1, size.y-1, m_radius);
#else
    dc.DrawRoundedRectangle(0, 0, size.x, size.y, m_radius);
#endif

    wxPoint pt = {10, 0};
    int width = m_icon.GetWidth();
    int height = m_icon.GetHeight();
    pt.y = (size.y - height) / 2;
#ifdef __APPLE__
    if(pt.y < 10){
        pt.y = 10;
    }
#endif //APPLE
    m_panel_page->SetPosition(pt);
    m_panel_page->Layout();
}

void VerifyCodeCtrl::SetRadius(int r) 
{ 
    m_radius = r;
    Refresh();
}

}
}
