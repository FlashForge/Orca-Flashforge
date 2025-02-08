#include "AmsPrintFileDlg.hpp"
#include "slic3r/GUI/FFUtils.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"

namespace Slic3r { namespace GUI {
    
AmsPrintFileDlg::AmsPrintFileDlg(wxWindow *parent)
    : TitleDialog(parent, _L("Print File"))
    , m_amsTipWnd(new AmsTipWnd(this))
    , m_comId(ComInvalidId)
{
    SetDoubleBuffered(true);
    SetBackgroundColour(*wxWHITE);
    SetForegroundColour(wxColour("#333333"));
    SetFont(wxGetApp().normal_font());
    SetMinSize(wxSize(FromDIP(420), -1));

    // top panel
    m_topPnl = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    wxBitmap timeBmp = create_scaled_bitmap("ff_print_time", this, 14);
    wxStaticBitmap *timeWxBmp = new wxStaticBitmap(m_topPnl, wxID_ANY, timeBmp, wxDefaultPosition, wxSize(FromDIP(16), FromDIP(16)));
    m_timeLbl = new wxStaticText(m_topPnl, wxID_ANY, wxEmptyString);

    wxBitmap weightBmp = create_scaled_bitmap("ff_print_weight", this, 14);
    wxStaticBitmap *weightWxBmp = new wxStaticBitmap(m_topPnl, wxID_ANY, weightBmp, wxDefaultPosition, wxSize(FromDIP(16), FromDIP(16)));
    m_weightLbl = new wxStaticText(m_topPnl, wxID_ANY, wxEmptyString);

    wxBoxSizer *timeWeightSizer = new wxBoxSizer(wxHORIZONTAL);
    timeWeightSizer->Add(timeWxBmp, 1, wxEXPAND | wxALL, FromDIP(5));
    timeWeightSizer->Add(m_timeLbl, 0, wxALL, FromDIP(5));
    timeWeightSizer->Add(0, 0, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    timeWeightSizer->Add(weightWxBmp, 1, wxEXPAND | wxALL, FromDIP(5));
    timeWeightSizer->Add(m_weightLbl, 0, wxALL, FromDIP(5));

    m_nameLbl = new wxStaticText(m_topPnl, wxID_ANY, wxEmptyString);
    m_nameLbl->SetMinSize(wxSize(FromDIP(315), FromDIP(24)));
    m_nameLbl->SetMaxSize(wxSize(FromDIP(315), FromDIP(24)));

    wxBoxSizer* rightTopSizer = new wxBoxSizer(wxVERTICAL);
    rightTopSizer->Add(m_nameLbl, 0, wxALIGN_LEFT | wxALIGN_BOTTOM, FromDIP(5));
    rightTopSizer->AddSpacer(FromDIP(5));
    rightTopSizer->Add(timeWeightSizer, 0, wxALIGN_LEFT | wxALIGN_TOP, 0);

    m_thumbWxBmp = new wxStaticBitmap(m_topPnl, wxID_ANY, wxNullBitmap);
    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);
    topSizer->Add(m_thumbWxBmp, 0, wxALIGN_CENTER_VERTICAL, 0);
    topSizer->AddSpacer(FromDIP(5));
    topSizer->Add(rightTopSizer, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT, 0);
    m_topPnl->SetSizer(topSizer);

    // material panel
    m_materialSizer = new wxGridSizer(0, 4, FromDIP(16), FromDIP(24));
    m_materialPnl = new wxPanel(this, wxID_ANY);
    m_materialPnl->SetSizer(m_materialSizer);

    m_amsTipLbl = new wxStaticText(this, wxID_ANY, _L("Please click the filament and select its corresponding slot\nbefore sending the print job."));
    m_amsTipLbl->SetForegroundColour(wxColour("#F59A23"));

    // print config
    m_levelChk = new FFCheckBox(this);
    m_levelChk->SetValue(false);
    m_levelChk->Bind(wxEVT_TOGGLEBUTTON, &AmsPrintFileDlg::onLevellingStateChanged,this);
    m_levelLbl = new wxStaticText(this, wxID_ANY, _L("Levelling"));

    m_flowCalibrationChk = new FFCheckBox(this);
    m_flowCalibrationChk->SetValue(false);
    m_flowCalibrationChk->Bind(wxEVT_TOGGLEBUTTON, &AmsPrintFileDlg::onFlowCalibrationStateChanged, this);
    m_flowCalibrationLbl = new wxStaticText(this, wxID_ANY, _L("Flow Calibration"));

    m_enableAmsChk = new FFCheckBox(this);
    m_enableAmsChk->SetValue(true);
    m_enableAmsChk->Bind(wxEVT_TOGGLEBUTTON, &AmsPrintFileDlg::onEnableAmsStateChanged, this);
    m_enableAmsLbl = new wxStaticText(this, wxID_ANY, _L("Enable IFS"));

    wxBitmap amsTipBmp = create_scaled_bitmap("ams_tutorial_icon", this, 16);
    m_amsTipWxBmp = new wxStaticBitmap(this, wxID_ANY, amsTipBmp, wxDefaultPosition, wxSize(FromDIP(16), FromDIP(16)), 0);
    m_amsTipWxBmp->Bind(wxEVT_ENTER_WINDOW, &AmsPrintFileDlg::onEnterAmsTipWidget, this);
    m_amsTipWxBmp->Bind(wxEVT_LEAVE_WINDOW, &AmsPrintFileDlg::onEnterAmsTipWidget, this);

    m_printConfigSizer = new wxBoxSizer(wxHORIZONTAL);

    // print button
    m_printBtn = new FFButton(this, wxID_ANY, _L("print"), FromDIP(4), false);
    m_printBtn->SetFontColor(wxColour("#ffffff"));
    m_printBtn->SetFontHoverColor(wxColor("#ffffff"));
    m_printBtn->SetFontPressColor(wxColor("#ffffff"));
    m_printBtn->SetFontDisableColor(wxColor("#ffffff"));
    m_printBtn->SetBGColor(wxColour("#419488"));
    m_printBtn->SetBGHoverColor(wxColour("#65A79E"));
    m_printBtn->SetBGPressColor(wxColour("#1A8676"));
    m_printBtn->SetBGDisableColor(wxColour("#dddddd"));
    m_printBtn->SetSize(wxSize(FromDIP(101), FromDIP(44)));
    m_printBtn->SetMinSize(wxSize(FromDIP(101), FromDIP(44)));
    m_printBtn->SetMaxSize(wxSize(FromDIP(101), FromDIP(44)));
    m_printBtn->Enable(false);
    m_printBtn->Bind(wxEVT_BUTTON, &AmsPrintFileDlg::onPrintButtonClicked, this);

    // main sizer
    wxBoxSizer *mainSizer = MainSizer();
    mainSizer->AddSpacer(FromDIP(12));
    mainSizer->Add(m_topPnl, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT, FromDIP(30));
    mainSizer->AddSpacer(FromDIP(12));
    mainSizer->Add(m_materialPnl, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(40));
    mainSizer->AddSpacer(FromDIP(22));
    mainSizer->Add(m_amsTipLbl, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(40));
    mainSizer->AddSpacer(FromDIP(22));
    mainSizer->Add(makeLineSpacer(), 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    mainSizer->AddSpacer(FromDIP(19));
    mainSizer->Add(m_printConfigSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    mainSizer->AddSpacer(FromDIP(19));
    mainSizer->Add(makeLineSpacer(), 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    mainSizer->AddSpacer(FromDIP(30));
    mainSizer->Add(m_printBtn, 0, wxEXPAND | wxALIGN_CENTER, FromDIP(40));
    mainSizer->AddSpacer(FromDIP(28));
    Layout();
    Fit();

    Centre(wxBOTH);
}

int AmsPrintFileDlg::ShowModal(com_local_job_data_t &jobData)
{
    int ret = TitleDialog::ShowModal();
    jobData.printNow = true;
    jobData.levelingBeforePrint = m_levelChk->GetValue();
    jobData.useMatlStation = m_enableAmsChk->GetValue();
    if (jobData.useMatlStation) {
        for (auto item : m_materialMapItems) {
            jobData.materialMappings.push_back(item->getMaterialMapping());
        }
    }
    return ret;
}

void AmsPrintFileDlg::setupData(com_id_t comId, const com_gcode_data_t &gcodeData, const wxImage &thumb)
{
    wxImage tmpImg = thumb;
    m_comId = comId;
    m_thumbWxBmp->SetBitmap(tmpImg.Rescale(FromDIP(108), FromDIP(117), wxIMAGE_QUALITY_BILINEAR));
    m_nameLbl->SetLabelText(wxString::FromUTF8(gcodeData.fileName));
    m_timeLbl->SetLabel(wxString::Format("%s", short_time(get_time_dhms(gcodeData.printingTime))));

    // weight
    char weight[64];
    if (wxGetApp().app_config->get("use_inches") == "1") {
        ::sprintf(weight, "  %.2f oz", gcodeData.totalFilamentWeight * 0.035274);
    } else {
        ::sprintf(weight, "  %.2f g", gcodeData.totalFilamentWeight);
    }
    m_weightLbl->SetLabel(weight);
    
    // materials
    m_materialSizer->Clear(true);
    m_materialMapItems.clear();
    for (size_t i = 0; i < gcodeData.gcodeToolDatas.size(); ++i) {
        int toolId = gcodeData.gcodeToolDatas[i].toolId;
        int slotId = gcodeData.gcodeToolDatas[i].slotId;
        wxColour color(gcodeData.gcodeToolDatas[i].materialColor);
        wxString name = wxString::FromUTF8(gcodeData.gcodeToolDatas[i].materialName);
        MaterialMapWgt *item = new MaterialMapWgt(m_materialPnl, toolId, color, name);
        item->setComId(comId);
        item->setupSlot(comId, slotId);
        item->Bind(SOLT_SELECT_EVENT, [this](SlotSelectEvent &) { updatePrintButtonState(); });
        item->Bind(SOLT_RESET_EVENT, [this](SlotResetEvent &) { updatePrintButtonState(); });
        m_materialSizer->Add(item);
        m_materialMapItems.push_back(item);
    }
    m_materialSizer->SetCols(std::min((int)gcodeData.gcodeToolDatas.size(), 4));

    // levelling
    if (wxGetApp().app_config->get("levelling").empty()) {
        m_levelChk->SetValue(false);
    } else {
        m_levelChk->SetValue(wxGetApp().app_config->get("levelling") == "true");
    }

    // flow calibration
    std::string modelId = FFUtils::getPrinterModelId(MultiComMgr::inst()->devData(comId).devDetail->pid);
    bool isSupportFlowCalibration = FFUtils::isPrinterSupportFlowCalibration(modelId);
    if (isSupportFlowCalibration) {
        if (wxGetApp().app_config->get("flowCalibration").empty()) {
            m_flowCalibrationChk->SetValue(false);
        } else {
            m_flowCalibrationChk->SetValue(wxGetApp().app_config->get("flowCalibration") == "true");
        }
    } else {
        m_flowCalibrationChk->SetValue(false);
    }
    m_flowCalibrationChk->Show(isSupportFlowCalibration);
    m_flowCalibrationLbl->Show(isSupportFlowCalibration);

    // print config layout
    m_printConfigSizer->Clear();
    m_printConfigSizer->Add(m_levelChk, 0, wxLEFT | wxALIGN_LEFT, FromDIP(10));
    m_printConfigSizer->Add(m_levelLbl, 0, wxLEFT | wxALIGN_LEFT, FromDIP(10));
    if (isSupportFlowCalibration) {
        m_printConfigSizer->AddStretchSpacer(1);
        m_printConfigSizer->Add(m_flowCalibrationChk, 0, wxLEFT | wxALIGN_LEFT, FromDIP(10));
        m_printConfigSizer->Add(m_flowCalibrationLbl, 0, wxLEFT | wxALIGN_LEFT, FromDIP(10));
    }
    m_printConfigSizer->AddStretchSpacer(1);
    m_printConfigSizer->Add(m_enableAmsChk, 0, wxLEFT | wxALIGN_LEFT, FromDIP(10));
    m_printConfigSizer->Add(m_enableAmsLbl, 0, wxLEFT | wxALIGN_LEFT, FromDIP(10));
    m_printConfigSizer->Add(m_amsTipWxBmp, 0, wxLEFT | wxALIGN_LEFT, FromDIP(10));
    if (isSupportFlowCalibration) {
        m_printConfigSizer->AddSpacer(FromDIP(10));
    } else {
        m_printConfigSizer->AddStretchSpacer(1);
    }
    updatePrintButtonState();

    // layout/fit
    m_topPnl->Layout();
    m_topPnl->Fit();
    Layout();
    Fit();
    m_materialPnl->Layout();
    m_materialPnl->Fit();
    Layout();
    Fit();
}

void AmsPrintFileDlg::onLevellingStateChanged(wxCommandEvent &event)
{
    event.Skip();
    if (m_levelChk->GetValue()) {
        wxGetApp().app_config->set("levelling", "true");
    } else {
        wxGetApp().app_config->set("levelling", "false");
    }
}

void AmsPrintFileDlg::onFlowCalibrationStateChanged(wxCommandEvent &event)
{
    event.Skip();
    if (m_flowCalibrationChk->GetValue()) {
        wxGetApp().app_config->set("flowCalibration", "true");
    } else {
        wxGetApp().app_config->set("flowCalibration", "false");
    }
}

void AmsPrintFileDlg::onEnableAmsStateChanged(wxCommandEvent &event)
{
    event.Skip();
    if (event.IsChecked()) {
        m_amsTipLbl->SetLabelText(_L("Please click the filament and select its corresponding slot\nbefore sending the print job."));
    } else {
        m_amsTipLbl->SetLabelText(_L("IFS not enabled, unable to select the slot"));
    }
    for (auto item : m_materialMapItems) {
        item->setEnable(event.IsChecked());
    }
    updatePrintButtonState();
}

void AmsPrintFileDlg::onEnterAmsTipWidget(wxMouseEvent& event)
{
    event.Skip();
    if (event.Entering()) {
        int y = m_amsTipWxBmp->GetRect().height + FromDIP(1);
        wxPoint pos = m_amsTipWxBmp->ClientToScreen(wxPoint(0, y));
        m_amsTipWnd->Move(pos);
        m_amsTipWnd->Show(true);
    } else {
        m_amsTipWnd->Show(false);
    }
}

void AmsPrintFileDlg::onPrintButtonClicked(wxCommandEvent &event)
{
    event.Skip();
    EndModal(wxID_OK);
}

void AmsPrintFileDlg::onConnectionExit(ComConnectionExitEvent &event)
{
    event.Skip();
    if (event.id == m_comId) {
        EndModal(wxID_CANCEL);
    }
}

void AmsPrintFileDlg::updatePrintButtonState()
{
    bool isAmsReady = true;
    if (m_enableAmsChk->GetValue()) {
        for (auto &item : m_materialMapItems) {
            if (!item->isSlotSelected()) {
                isAmsReady = false;
                break;
            }
        }
    }
    m_printBtn->Enable(isAmsReady);
}

wxPanel *AmsPrintFileDlg::makeLineSpacer()
{
    wxPanel *pnl = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    pnl->SetForegroundColour(wxColour("#dddddd"));
    pnl->SetBackgroundColour(wxColour("#dddddd"));
    return pnl;
}

}} // namespace Slic3r::GUI
