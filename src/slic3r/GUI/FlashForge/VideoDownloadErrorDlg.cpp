#include "VideoDownloadErrorDlg.hpp"

#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "../wxExtensions.hpp"

namespace Slic3r {
namespace GUI {

VideoDownloadErrorDlg::VideoDownloadErrorDlg(wxWindow* parent, const std::vector<wxString>& file_infos)
    : wxDialog(parent, wxID_ANY, _L("Tip"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
    , m_file_infos(file_infos)
{
    SetBackgroundColour("#FFFFFF");
    m_sizer_main = new wxBoxSizer(wxVERTICAL);
    m_sizer_main->SetMinSize(wxSize(FromDIP(260), FromDIP(296)));


    m_msg_Lbl = new wxStaticText(this, wxID_ANY, _L("Unable to download some videos. Please try again later!"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    m_msg_Lbl->SetFont(::Label::Body_16);
    m_msg_Lbl->SetForegroundColour("#333333");

    m_list_Lbl = new wxStaticText(this, wxID_ANY, _L("Failed videos:"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    m_list_Lbl->SetFont(::Label::Body_14);
    m_list_Lbl->SetForegroundColour("#333333");


    m_btn_confirm = new FFButton(this, wxID_ANY, wxEmptyString);
    m_btn_confirm->SetLabel(_L("Confirm"), FromDIP(64), FromDIP(35));
    m_btn_confirm->SetFontColor(*wxWHITE);
    m_btn_confirm->SetBGColor("#419488");
    m_btn_confirm->SetBorderColor("#419488");
    m_btn_confirm->SetFontHoverColor(*wxWHITE);
    m_btn_confirm->SetBGHoverColor("#65A79E");
    m_btn_confirm->SetBorderHoverColor("#65A79E");
    m_btn_confirm->SetFontPressColor(*wxWHITE);
    m_btn_confirm->SetBGPressColor("#1A8676");
    m_btn_confirm->SetBorderPressColor("#1A8676");
    m_btn_confirm->SetFontDisableColor(*wxWHITE);
    m_btn_confirm->SetBGDisableColor("#dddddd");
    m_btn_confirm->SetBorderDisableColor("#dddddd");


    m_scroll_wgt = new wxScrolledWindow(this, wxID_ANY);
    m_scroll_wgt->SetSize(wxSize(FromDIP(304), FromDIP(356)));

    m_scroll_wgt->SetVirtualSize(1000, 500);
    m_scroll_wgt->SetScrollRate(10, 10);

    m_sizer_scroll = new wxBoxSizer(wxVERTICAL);
    for (int i = 0; i < m_file_infos.size(); i++) {
        const wxString &file_info = m_file_infos.at(i);
        wxBoxSizer* sizer_item = new wxBoxSizer(wxHORIZONTAL);
        wxBitmap    error_bmp(create_scaled_bitmap("video_download_error", nullptr, 16));
        wxStaticBitmap* staticBitmap = new wxStaticBitmap(m_scroll_wgt, wxID_ANY, error_bmp);
        wxStaticText*   file_Lbl     = new wxStaticText(m_scroll_wgt, wxID_ANY, file_info,
                                                 wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT | wxALIGN_CENTER);
        file_Lbl->SetFont(::Label::Body_14);
        file_Lbl->SetForegroundColour("#333333");
        sizer_item->Add(staticBitmap);
        sizer_item->AddSpacer(FromDIP(4));
        sizer_item->Add(file_Lbl);
        m_sizer_scroll->AddSpacer(FromDIP(10));
        m_sizer_scroll->Add(sizer_item);
    }
    m_scroll_wgt->SetSizer(m_sizer_scroll);


    wxSizer* sizer_btn = new wxBoxSizer(wxHORIZONTAL);
    sizer_btn->AddStretchSpacer(1);
    sizer_btn->Add(m_btn_confirm);
    sizer_btn->AddStretchSpacer(1);


    m_sizer_main->AddSpacer(FromDIP(30));
    m_sizer_main->Add(m_msg_Lbl, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(15));
    m_sizer_main->AddSpacer(FromDIP(20));
    m_sizer_main->Add(m_list_Lbl, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(15));
    m_sizer_main->Add(m_scroll_wgt, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(15));
    m_sizer_main->AddSpacer(FromDIP(32));
    m_sizer_main->Add(sizer_btn, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(15));
    m_sizer_main->AddSpacer(FromDIP(32));

    SetSizer(m_sizer_main);

    //SetSize(wxSize(400, 300));
    this->Layout();
    this->Fit();
    CentreOnParent();
    Layout();

    m_btn_confirm->Bind(wxEVT_BUTTON, ([this](wxCommandEvent& event) { EndModal(wxOK); }));

}

VideoDownloadErrorDlg::~VideoDownloadErrorDlg()
{

}

}} // namespace Slic3r::GUI