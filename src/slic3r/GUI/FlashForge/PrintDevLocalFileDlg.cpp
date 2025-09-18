#include "PrintDevLocalFileDlg.hpp"
#include <cstring>
#include "libslic3r/Utils.hpp"
#include "slic3r/GUI/FFUtils.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"

namespace Slic3r { namespace GUI {
    
PrintDevLocalFileDlg::PrintDevLocalFileDlg(wxWindow *parent)
    : DPIDialog(parent, wxID_ANY, _L("Print File"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
    , m_amsTipWnd(new AmsTipWnd(this))
    , m_comId(ComInvalidId)
    , m_isSupportLidar(false)
    , m_isSupportCamera(false)
{
    std::string icoPath = (boost::format("%1%/images/Orca-FlashforgeTitle.ico") % resources_dir()).str();
    SetDoubleBuffered(true);
    SetBackgroundColour(*wxWHITE);
    SetForegroundColour(wxColour("#333333"));
    SetFont(wxGetApp().normal_font());
    SetIcon(wxIcon(encode_path(icoPath.c_str()), wxBITMAP_TYPE_ICO));
    SetMinSize(wxSize(FromDIP(420), -1));

    // top panel
    m_topPnl = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    wxBitmap timeBmp = create_scaled_bitmap("ff_print_time", this, 14);
    m_timeWxBmp = new wxStaticBitmap(m_topPnl, wxID_ANY, timeBmp, wxDefaultPosition, wxSize(FromDIP(16), FromDIP(16)));
    m_timeLbl = new wxStaticText(m_topPnl, wxID_ANY, wxEmptyString);

    wxBitmap weightBmp = create_scaled_bitmap("ff_print_weight", this, 14);
    m_weightWxBmp = new wxStaticBitmap(m_topPnl, wxID_ANY, weightBmp, wxDefaultPosition, wxSize(FromDIP(16), FromDIP(16)));
    m_weightLbl = new wxStaticText(m_topPnl, wxID_ANY, wxEmptyString);

    wxBoxSizer *timeWeightSizer = new wxBoxSizer(wxHORIZONTAL);
    timeWeightSizer->Add(m_timeWxBmp, 1, wxEXPAND | wxALL, FromDIP(5));
    timeWeightSizer->Add(m_timeLbl, 0, wxALL, FromDIP(5));
    timeWeightSizer->Add(0, 0, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    timeWeightSizer->Add(m_weightWxBmp, 1, wxEXPAND | wxALL, FromDIP(5));
    timeWeightSizer->Add(m_weightLbl, 0, wxALL, FromDIP(5));

    m_nameLbl = new wxStaticText(m_topPnl, wxID_ANY, wxEmptyString);
    m_nameLbl->SetMinSize(wxSize(FromDIP(315), FromDIP(24)));
    m_nameLbl->SetMaxSize(wxSize(FromDIP(315), FromDIP(24)));

    wxBoxSizer* rightTopSizer = new wxBoxSizer(wxVERTICAL);
    rightTopSizer->Add(m_nameLbl, 0, wxALIGN_LEFT | wxALIGN_BOTTOM, FromDIP(5));
    rightTopSizer->AddSpacer(FromDIP(5));
    rightTopSizer->Add(timeWeightSizer, 0, wxALIGN_LEFT | wxALIGN_TOP, 0);

    m_thumbWxBmp = new wxStaticBitmap(m_topPnl, wxID_ANY, wxNullBitmap);
    m_thumbWxBmp->SetMinSize(wxSize(FromDIP(108), FromDIP(117)));
    m_thumbWxBmp->SetMaxSize(wxSize(FromDIP(108), FromDIP(117)));
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
    m_levelChk->Bind(wxEVT_TOGGLEBUTTON, &PrintDevLocalFileDlg::onLevellingStateChanged,this);
    m_levelLbl = new wxStaticText(this, wxID_ANY, _L("Levelling"));

    m_enableAmsChk = new FFCheckBox(this);
    m_enableAmsChk->SetValue(true);
    m_enableAmsChk->Bind(wxEVT_TOGGLEBUTTON, &PrintDevLocalFileDlg::onEnableAmsStateChanged, this);
    m_enableAmsLbl = new wxStaticText(this, wxID_ANY, _L("Enable IFS"));

    wxBitmap amsTipBmp = create_scaled_bitmap("ams_tutorial_icon", this, 16);
    m_amsTipWxBmp = new wxStaticBitmap(this, wxID_ANY, amsTipBmp, wxDefaultPosition, wxSize(FromDIP(16), FromDIP(16)), 0);
    m_amsTipWxBmp->Bind(wxEVT_ENTER_WINDOW, &PrintDevLocalFileDlg::onEnterAmsTipWidget, this);
    m_amsTipWxBmp->Bind(wxEVT_LEAVE_WINDOW, &PrintDevLocalFileDlg::onEnterAmsTipWidget, this);

    m_flowCalibrationChk = new FFCheckBox(this);
    m_flowCalibrationChk->SetValue(false);
    m_flowCalibrationChk->Bind(wxEVT_TOGGLEBUTTON, &PrintDevLocalFileDlg::onFlowCalibrationStateChanged, this);
    m_flowCalibrationLbl = new wxStaticText(this, wxID_ANY, _CTX("Flow Calibration", "Flashforge"));

    m_firstLayerInspectionChk = new FFCheckBox(this);
    m_firstLayerInspectionChk->SetValue(false);
    m_firstLayerInspectionChk->Bind(wxEVT_TOGGLEBUTTON, &PrintDevLocalFileDlg::onFirstLayerInspectionStateChanged, this);
    m_firstLayerInspectionLbl = new wxStaticText(this, wxID_ANY, _CTX("First Layer Inspection", "Flashforge"));
    m_firstLayerInspectionLbl->SetForegroundColour(wxColour("#333333"));

    m_timeLapseVideoChk = new FFCheckBox(this);
    m_timeLapseVideoChk->SetValue(false);
    m_timeLapseVideoChk->Bind(wxEVT_TOGGLEBUTTON, &PrintDevLocalFileDlg::onTimeLapseVideoStateChanged, this);
    m_timeLapseVideoLbl = new wxStaticText(this, wxID_ANY, _L("Time-Lapse Video"));
    m_timeLapseVideoLbl->SetForegroundColour(wxColour("#333333"));

    m_printConfigSizer = new wxFlexGridSizer(3, FromDIP(10), FromDIP(10));

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
    m_printBtn->Bind(wxEVT_BUTTON, &PrintDevLocalFileDlg::onPrintButtonClicked, this);

    // main sizer
    m_mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(m_mainSizer);

    MultiComMgr::inst()->Bind(COM_CONNECTION_EXIT_EVENT, &PrintDevLocalFileDlg::onConnectionExit, this);
    MultiComMgr::inst()->Bind(COM_WAN_DEV_INFO_UPDATE_EVENT, &PrintDevLocalFileDlg::onWanDevInfoUpdate, this);
    MultiComMgr::inst()->Bind(COM_DEV_DETAIL_UPDATE_EVENT, &PrintDevLocalFileDlg::onDevDetailUpdate, this);
}

int PrintDevLocalFileDlg::ShowModal(com_local_job_data_t &jobData)
{
    int ret = DPIDialog::ShowModal();
    jobData.printNow = true;
    jobData.levelingBeforePrint = m_levelChk->GetValue();
    jobData.useMatlStation = m_enableAmsChk->GetValue();
    jobData.flowCalibration = m_flowCalibrationChk->GetValue();
    jobData.firstLayerInspection = m_firstLayerInspectionChk->GetValue();
    jobData.timeLapseVideo = m_timeLapseVideoChk->GetValue();
    if (jobData.useMatlStation) {
        for (auto item : m_materialMapItems) {
            jobData.materialMappings.push_back(item->getMaterialMapping());
        }
    }
    return ret;
}

bool PrintDevLocalFileDlg::setupData(com_id_t comId, const com_gcode_data_t &gcodeData, const wxImage &thumb)
{
    bool valid = false;
    const fnet_dev_detail_t *devDetail = MultiComMgr::inst()->devData(comId, &valid).devDetail;
    if (!valid) {
        return false;
    }
    wxImage tmpImg = thumb;
    m_comId = comId;
    m_thumbWxBmp->SetBitmap(tmpImg.Rescale(m_thumbWxBmp->GetMinWidth(), m_thumbWxBmp->GetMinHeight(), wxIMAGE_QUALITY_BILINEAR));
    m_nameLbl->SetLabelText(wxString::FromUTF8(gcodeData.fileName));

    // time
    bool showTimeWeight = gcodeData.totalFilamentWeight > 1e-6;
    m_timeLbl->SetLabel(wxString::Format("%s", short_time(get_time_dhms(gcodeData.printingTime))));
    m_timeWxBmp->Show(showTimeWeight);
    m_timeLbl->Show(showTimeWeight);

    // weight
    char weight[64];
    if (wxGetApp().app_config->get("use_inches") == "1") {
        ::sprintf(weight, "  %.2f oz", gcodeData.totalFilamentWeight * 0.035274);
    } else {
        ::sprintf(weight, "  %.2f g", gcodeData.totalFilamentWeight);
    }
    m_weightLbl->SetLabel(weight);
    m_weightWxBmp->Show(showTimeWeight);
    m_weightLbl->Show(showTimeWeight);
    
    // materials
    m_materialSizer->Clear(true);
    m_materialMapItems.clear();
    if (gcodeData.useMatlStation) {
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
        m_amsTipLbl->Show(!gcodeData.gcodeToolDatas.empty());
    } else {
        m_amsTipLbl->Show(false);
    }

    // levelling
    if (wxGetApp().app_config->get("levelling").empty()) {
        m_levelChk->SetValue(false);
    } else {
        m_levelChk->SetValue(wxGetApp().app_config->get("levelling") == "true");
    }

    // AMS
    bool useAms = FFUtils::isPrinterSupportAms(FFUtils::getPrinterModelId(devDetail->pid));
    m_enableAmsChk->SetValue(wxString::FromUTF8(gcodeData.fileName).Right(4).IsSameAs(".3mf", false));
    m_enableAmsChk->Show(useAms);
    m_enableAmsLbl->Show(useAms);
    m_amsTipWxBmp->Show(useAms);

    updateConfigState(devDetail, true);
    updatePrintButtonState();

    // layout
    m_mainSizer->Clear();
    if (useAms) {
        m_mainSizer->AddSpacer(FromDIP(12));
        m_mainSizer->Add(m_topPnl, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT, FromDIP(30));
        m_mainSizer->AddSpacer(FromDIP(12));
        m_mainSizer->Add(m_materialPnl, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(40));
        m_mainSizer->AddSpacer(FromDIP(22));
        m_mainSizer->Add(m_amsTipLbl, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(40));
        m_mainSizer->AddSpacer(FromDIP(22));
        m_mainSizer->Add(makeLineSpacer(), 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
        m_mainSizer->AddSpacer(FromDIP(19));
        m_mainSizer->Add(m_printConfigSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(40));
        m_mainSizer->AddSpacer(FromDIP(40));
        m_mainSizer->Add(m_printBtn, 0, wxEXPAND | wxALIGN_CENTER, FromDIP(40));
        m_mainSizer->AddSpacer(FromDIP(28));
    } else {
        m_mainSizer->AddSpacer(FromDIP(12));
        m_mainSizer->Add(m_topPnl, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT, FromDIP(30));
        m_mainSizer->AddSpacer(FromDIP(22));
        m_mainSizer->Add(makeLineSpacer(), 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
        m_mainSizer->AddSpacer(FromDIP(19));
        m_mainSizer->Add(m_printConfigSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(40));
        m_mainSizer->AddSpacer(FromDIP(40));
        m_mainSizer->Add(m_printBtn, 0, wxEXPAND | wxALIGN_CENTER, FromDIP(40));
        m_mainSizer->AddSpacer(FromDIP(28));
    }
    m_topPnl->Layout();
    m_topPnl->Fit();
    Layout();
    Fit();
    m_materialPnl->Layout();
    m_materialPnl->Fit();
    Layout();
    Fit();
    CenterOnParent(wxBOTH);
    return true;
}

void PrintDevLocalFileDlg::onLevellingStateChanged(wxCommandEvent &event)
{
    event.Skip();
    if (m_levelChk->GetValue()) {
        wxGetApp().app_config->set("levelling", "true");
    } else {
        wxGetApp().app_config->set("levelling", "false");
    }
}

void PrintDevLocalFileDlg::onEnableAmsStateChanged(wxCommandEvent &event)
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

void PrintDevLocalFileDlg::onEnterAmsTipWidget(wxMouseEvent& event)
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

void PrintDevLocalFileDlg::onFlowCalibrationStateChanged(wxCommandEvent &event)
{
    event.Skip();
    if (m_flowCalibrationChk->GetValue()) {
        wxGetApp().app_config->set("flowCalibration", "true");
    } else {
        wxGetApp().app_config->set("flowCalibration", "false");
    }
}

void PrintDevLocalFileDlg::onFirstLayerInspectionStateChanged(wxCommandEvent& event)
{
    event.Skip();
    if (m_firstLayerInspectionChk->GetValue()) {
        wxGetApp().app_config->set("firstLayerInspection", "true");
    } else {
        wxGetApp().app_config->set("firstLayerInspection", "false");
    }
}

void PrintDevLocalFileDlg::onTimeLapseVideoStateChanged(wxCommandEvent& event)
{
    event.Skip();
    if (m_timeLapseVideoChk->GetValue()) {
        wxGetApp().app_config->set("timeLapseVideo", "true");
    } else {
        wxGetApp().app_config->set("timeLapseVideo", "false");
    }
}

void PrintDevLocalFileDlg::onPrintButtonClicked(wxCommandEvent &event)
{
    event.Skip();
    EndModal(wxID_OK);
}

void PrintDevLocalFileDlg::onConnectionExit(ComConnectionExitEvent &event)
{
    event.Skip();
    if (event.id == m_comId) {
        EndModal(wxID_CANCEL);
    }
}

void PrintDevLocalFileDlg::onWanDevInfoUpdate(ComWanDevInfoUpdateEvent &event)
{
    event.Skip();
    if (event.id != m_comId) {
        return;
    }
    bool valid;
    const com_dev_data_t &devData = MultiComMgr::inst()->devData(event.id, &valid);
    if (valid && devData.connectMode == COM_CONNECT_WAN && devData.wanDevInfo.status == "offline") {
        EndModal(wxID_CANCEL);
    }
}

void PrintDevLocalFileDlg::onDevDetailUpdate(ComDevDetailUpdateEvent &event)
{
    event.Skip();
    if (event.id != m_comId) {
        return;
    }
    bool valid;
    const com_dev_data_t &devData = MultiComMgr::inst()->devData(event.id, &valid);
    if (!valid) {
        return;
    }
    if (updateConfigState(devData.devDetail)) {
        Layout();
        Fit();
    }
}

bool PrintDevLocalFileDlg::updateConfigState(const fnet_dev_detail_t *devDetail, bool isInit /* = false */)
{
    bool isSupportLidar = devDetail->lidar == 1;
    bool isSupportCamera = devDetail->camera == 1;
    if (!isInit && isSupportLidar == m_isSupportLidar && isSupportCamera == m_isSupportCamera) {
        return false;
    }
    m_isSupportLidar = isSupportLidar;
    m_isSupportCamera = isSupportCamera;

    // flow calibration
    if (!isSupportLidar) {
        m_flowCalibrationChk->SetValue(false);
    } else {
        std::string value = wxGetApp().app_config->get("flowCalibration");
        m_flowCalibrationChk->SetValue(value.empty() || value == "true");
    }
    m_flowCalibrationChk->Show(isSupportLidar);
    m_flowCalibrationLbl->Show(isSupportLidar);

    // first layer inspection
    if (!isSupportLidar) {
        m_firstLayerInspectionChk->SetValue(false);
    } else {
        std::string value = wxGetApp().app_config->get("firstLayerInspection");
        m_firstLayerInspectionChk->SetValue(value.empty() || value == "true");
    }
    m_firstLayerInspectionChk->Show(isSupportLidar);
    m_firstLayerInspectionLbl->Show(isSupportLidar);

    // time lapse video
    if (!isSupportCamera || wxGetApp().app_config->get("timeLapseVideo").empty()) {
        m_timeLapseVideoChk->SetValue(false);
    } else {
        m_timeLapseVideoChk->SetValue(wxGetApp().app_config->get("timeLapseVideo") == "true");
    }
    m_timeLapseVideoChk->Show(isSupportCamera);
    m_timeLapseVideoLbl->Show(isSupportCamera);

    // print config layout
    std::vector<std::pair<FFCheckBox*, wxStaticText*>> configPairs;
    configPairs.emplace_back(m_levelChk, m_levelLbl);
    if (FFUtils::isPrinterSupportAms(FFUtils::getPrinterModelId(devDetail->pid))) {
        configPairs.emplace_back(m_enableAmsChk, m_enableAmsLbl);
    }
    if (isSupportLidar) {
        configPairs.emplace_back(m_flowCalibrationChk, m_flowCalibrationLbl);
    }
    if (isSupportLidar) {
        configPairs.emplace_back(m_firstLayerInspectionChk, m_firstLayerInspectionLbl);
    }
    if (isSupportCamera) {
        configPairs.emplace_back(m_timeLapseVideoChk, m_timeLapseVideoLbl);
    }
    m_printConfigSizer->Clear();
    for (size_t i = 0; i < configPairs.size(); ++i) {
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(configPairs[i].first, 0, wxALIGN_LEFT);
        sizer->Add(configPairs[i].second, 0, wxLEFT | wxALIGN_LEFT, FromDIP(10));
        if (configPairs[i].first == m_enableAmsChk) {
            sizer->Add(m_amsTipWxBmp, 0, wxLEFT | wxALIGN_LEFT, FromDIP(10));
        }
        m_printConfigSizer->Add(sizer);
    }
    m_printConfigSizer->SetCols(configPairs.size() >= 3 ? 3 : 2);
    m_printConfigSizer->AddGrowableCol(0, 1);
    m_printConfigSizer->AddGrowableCol(1, 1);
    m_printConfigSizer->Layout();
    return true;
}

void PrintDevLocalFileDlg::updatePrintButtonState()
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

wxPanel *PrintDevLocalFileDlg::makeLineSpacer()
{
    wxPanel *pnl = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    pnl->SetForegroundColour(wxColour("#dddddd"));
    pnl->SetBackgroundColour(wxColour("#dddddd"));
    return pnl;
}

}} // namespace Slic3r::GUI
