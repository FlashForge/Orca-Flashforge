#include "PrinterErrorMsgDlg.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"

namespace Slic3r { namespace GUI {

PrinterErrorMsgDlg::PrinterErrorMsgDlg(wxWindow *parent, com_id_t comId, const std::string &errorCode)
    : wxDialog(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU)
    , m_comId(comId)
    , m_errorCode(errorCode)
{
    SetBackgroundColour(*wxWHITE);
    SetDoubleBuffered(true);

    m_titleLbl = new wxStaticText(this, wxID_ANY, _L("Error"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    m_titleLbl->SetFont(::Label::Body_14);
    m_titleLbl->SetForegroundColour("#333333");

    m_msgLbl = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    m_msgLbl->SetMinSize(wxSize(FromDIP(361), -1));
    m_msgLbl->SetForegroundColour("#333333");

    m_continueBtn = new FFButton(this, wxID_ANY, wxEmptyString);
    m_continueBtn->SetFontColor("#419488");
    m_continueBtn->SetBorderColor("#419488");
    m_continueBtn->SetFontHoverColor("#65A79E");
    m_continueBtn->SetBorderHoverColor("#65A79E");
    m_continueBtn->SetFontPressColor("#1A8676");
    m_continueBtn->SetBorderPressColor("#1A8676");

    m_stopBtn = new FFButton(this, wxID_ANY, wxEmptyString);
    m_stopBtn->SetFontColor("#419488");
    m_stopBtn->SetBorderColor("#419488");
    m_stopBtn->SetFontHoverColor("#65A79E");
    m_stopBtn->SetBorderHoverColor("#65A79E");
    m_stopBtn->SetFontPressColor("#1A8676");
    m_stopBtn->SetBorderPressColor("#1A8676");

    wxSizer *sizerBtn = new wxBoxSizer(wxHORIZONTAL);
    sizerBtn->AddStretchSpacer(1);
    sizerBtn->Add(m_continueBtn);
    sizerBtn->AddSpacer(FromDIP(23));
    sizerBtn->Add(m_stopBtn);
    sizerBtn->AddStretchSpacer(1);

    wxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddSpacer(FromDIP(34));
    sizer->Add(m_titleLbl, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(15));
    sizer->AddSpacer(FromDIP(15));
    sizer->Add(m_msgLbl, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(15));
    sizer->AddSpacer(FromDIP(34));
    sizer->Add(sizerBtn, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(15));
    sizer->AddSpacer(FromDIP(47));
    SetSizer(sizer);

    setupErrorCode(errorCode);
    m_continueBtn->Bind(wxEVT_BUTTON, &PrinterErrorMsgDlg::onContinue, this);
    m_stopBtn->Bind(wxEVT_BUTTON, &PrinterErrorMsgDlg::onStop, this);
    MultiComMgr::inst()->Bind(COM_CONNECTION_EXIT_EVENT, &PrinterErrorMsgDlg::onConnectionExit, this);
    MultiComMgr::inst()->Bind(COM_DEV_DETAIL_UPDATE_EVENT, &PrinterErrorMsgDlg::onDevDetailUpdate, this);

    Layout();
    Fit();
    CenterOnScreen();
}

void PrinterErrorMsgDlg::setupErrorCode(const std::string &errorCode)
{
    if (errorCode == "E0088") {
        m_msgLbl->SetLabelText(_L("Non-Flashforge build plate detected. Print quality may not be guaranteed."));
        m_continueBtn->SetLabel(_L("Continue printing"), FromDIP(165), FromDIP(36));
        m_stopBtn->SetLabel(_L("Stop printing (replace the build plate)"), FromDIP(165), FromDIP(36));
    } else if (errorCode == "E0089") {
        m_msgLbl->SetLabelText(_L("Lidar detected first-layer defects. Please check and decide whether to continue printing."));
        m_continueBtn->SetLabel(_L("Continue printing (defects acceptable)"), FromDIP(165), FromDIP(36));
        m_stopBtn->SetLabel(_L("Stop printing"), FromDIP(165), FromDIP(36));
    }
    if (!m_msgLbl->GetLabelText().empty()) {
        Layout();
        Fit();
        m_msgLbl->Wrap(m_msgLbl->GetSize().x);
    }
}

void PrinterErrorMsgDlg::onContinue(wxCommandEvent &event)
{
    event.Skip();
    if (m_errorCode == "E0088") {
        MultiComMgr::inst()->putCommand(m_comId, new ComPlateDetectCtrl("continue"));
    } else if (m_errorCode == "E0089") {
        MultiComMgr::inst()->putCommand(m_comId, new ComFirstLayerDetectCtrl("continue"));
    }
    EndModal(wxOK);
}

void PrinterErrorMsgDlg::onStop(wxCommandEvent &event)
{
    event.Skip();
    if (m_errorCode == "E0088") {
        MultiComMgr::inst()->putCommand(m_comId, new ComPlateDetectCtrl("stop"));
    } else if (m_errorCode == "E0089") {
        MultiComMgr::inst()->putCommand(m_comId, new ComFirstLayerDetectCtrl("stop"));
    }
    EndModal(wxOK);
}

void PrinterErrorMsgDlg::onConnectionExit(ComConnectionExitEvent &event)
{
    event.Skip();
    if (event.id != m_comId) {
        return;
    }
    EndModal(wxCANCEL);
}

void PrinterErrorMsgDlg::onDevDetailUpdate(ComDevDetailUpdateEvent &event)
{
    event.Skip();
    if (event.id != m_comId) {
        return;
    }
    if (strcmp(event.devDetail->status, "error") != 0 || event.devDetail->errorCode != m_errorCode) {
        EndModal(wxCANCEL);
    }
}

}} // namespace Slic3r::GUI
