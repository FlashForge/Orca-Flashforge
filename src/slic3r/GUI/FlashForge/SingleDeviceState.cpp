#include "SingleDeviceState.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/BitmapCache.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include <slic3r/GUI/Widgets/WebView.hpp>
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"
#include "slic3r/GUI/FlashForge/PrintDevLocalFileDlg.hpp"
#include "slic3r/GUI/FlashForge/PrinterErrorMsgDlg.hpp"
#include <nlohmann/json.hpp>
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/Plater.hpp"
#include "slic3r/GUI/FFUtils.hpp"
#include <wx/dcgraph.h>
using namespace std::literals;
using json   = nlohmann::json;
namespace pt = boost::property_tree;

namespace Slic3r {
namespace GUI {

wxDEFINE_EVENT(EVT_SWITCH_TO_FILETER, wxCommandEvent);

const std::string CLOSE = "close";
const std::string OPEN  = "open";
const std::string CANCEL ="cancel";
const std::string PAUSE  = "pause";
const std::string CONTINUE = "continue";
const std::string CLEAR_PLATFORM = "setClearPlatform";

const std::string OPENSTREAM = "streamCtrl_cmd";

const std::string P_READY = "ready"; 
const std::string P_COMPLETED = "completed";
const std::string P_ERROR     = "error";
const std::string P_PAUSING  = "pausing";
const std::string P_BUSY      = "busy";
const std::string P_HEATING   = "heating";
const std::string P_CALIBRATE  = "calibrate_doing";

const wxString    TEMPERATURE = _L("Temperature");
const wxString    TEMP_CANCEL  = _L("cancel");
const wxString    TEMP_CONFIRM = _L("confirm");

const wxString    HAS_NO_PRINTING = _L("The current device has \nno printing projects");

const int TEXT_LENGTH = 15;
const int MATERIAL_PIC_WIDTH  = 80;
const int MATERIAL_PIC_HEIGHT = 80;
const int IDLE_NAME_LENGTH    = 150;

const int FILELIST_PIC_WIDTH = 41;
const int FILELIST_PIC_HEIGHT = 45;

MaterialImagePanel::MaterialImagePanel(wxWindow *parent, const wxSize &size /*=wxDefaultSize*/)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
{
    Bind(wxEVT_PAINT, &MaterialImagePanel::OnPaint, this);
    // Bind(wxEVT_SIZE, &RoundImagePanel::OnSize, this);
}

void MaterialImagePanel::SetImage(const wxImage &image)
{
    m_image = image;
#ifdef __WIN32__
    Refresh();
#endif
    
}

void MaterialImagePanel::OnSize(wxSizeEvent &event) {}

void MaterialImagePanel::OnPaint(wxPaintEvent &event)
{
    if (!m_image.IsOk()) {
        event.Skip();
        return;
    }

    wxSize  size = GetSize();

    wxImage img  = m_image;
    img.Rescale(size.x, size.y);
    wxPaintDC dc(this);

    //if (!img.HasAlpha()) {
    //    img.InitAlpha();
    //}

    wxBitmap  bmp(size.x, size.y);
    {
        wxMemoryDC memdc;
        memdc.SelectObject(bmp);
#ifdef _WIN32
        memdc.Blit({0, 0}, size, &dc, {0, 0});
#endif
        wxGCDC dc2(memdc);
        dc2.SetFont(GetFont());
        CreateRegion(dc2);
        memdc.SelectObject(wxNullBitmap);
    }
    wxImage ref_img = bmp.ConvertToImage();
    for (int y = 0; y < img.GetHeight(); ++y) {
        for (int x = 0; x < img.GetWidth(); ++x) {
            img.SetAlpha(x, y, ref_img.GetRed(x, y));
        }
    }
    dc.DrawBitmap(wxBitmap(img), 0, 0);
}

void MaterialImagePanel::CreateRegion(wxDC &dc)
{
    wxSize sz = GetSize();
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(0, 0, sz.x, sz.y);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(*wxRED);
    dc.DrawRoundedRectangle(0, 0, sz.x, sz.y,10);
}

StartFilter::StartFilter(wxWindow* parent)
    : wxPanel(parent, wxID_ANY,wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
    SetBackgroundColour(*wxWHITE);
    create_panel(this);
}

StartFilter::~StartFilter()
{
}

void StartFilter::create_panel(wxWindow* parent)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *bSizer_filtering_title = new wxBoxSizer(wxHORIZONTAL);

    auto m_panel_filtering_title = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_filtering_title->SetBackgroundColour(wxColour(248,248,248));
    m_panel_filtering_title->SetMinSize(wxSize(FromDIP(450), FromDIP(36)));

    //过滤标题
    auto m_staticText_filtering = new wxStaticText(m_panel_filtering_title, wxID_ANY ,_L("Start Filtering"));
    m_staticText_filtering->SetForegroundColour(wxColour(51,51,51));

    bSizer_filtering_title->Add(m_staticText_filtering, 0, wxLEFT | wxCENTER, FromDIP(17));
    m_panel_filtering_title->SetSizer(bSizer_filtering_title);
    m_panel_filtering_title->Layout();
    bSizer_filtering_title->Fit(m_panel_filtering_title);

    //内循环过滤
    wxBoxSizer *bSizer_internal_circulate_hor = new wxBoxSizer(wxHORIZONTAL);
    wxPanel*    internal_circulate_panel      = new wxPanel(parent, wxID_ANY, wxDefaultPosition,wxDefaultSize, wxTAB_TRAVERSAL);
    auto m_staticText_internal_circulate = new wxStaticText(internal_circulate_panel, wxID_ANY, _L("Internal Circulate"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    //m_staticText_internal_circulate->SetFont(wxFont(wxFontInfo(16)));
    m_internal_circulate_switch = new SwitchButton(internal_circulate_panel);
    m_internal_circulate_switch->SetBackgroundColour(*wxWHITE);
    m_internal_circulate_switch->Bind(wxEVT_TOGGLEBUTTON, &StartFilter::onAirFilterToggled, this);

    bSizer_internal_circulate_hor->AddSpacer(FromDIP(17));
    bSizer_internal_circulate_hor->Add(m_staticText_internal_circulate, 0, wxLEFT, 0);
    bSizer_internal_circulate_hor->AddSpacer(FromDIP(7));
    bSizer_internal_circulate_hor->Add(m_internal_circulate_switch, 0, wxLEFT, 0);

    internal_circulate_panel->SetSizer(bSizer_internal_circulate_hor);
    internal_circulate_panel->Layout();
    bSizer_internal_circulate_hor->Fit(internal_circulate_panel);

    //外循环过滤
    wxBoxSizer *bSizer_external_circulate_hor = new wxBoxSizer(wxHORIZONTAL);
    wxPanel*    external_circulate_panel      = new wxPanel(parent, wxID_ANY, wxDefaultPosition,wxDefaultSize, wxTAB_TRAVERSAL);
    auto m_staticText_external_circulate = new wxStaticText(external_circulate_panel, wxID_ANY, _L("External Circulate"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    //m_staticText_external_circulate->SetFont(wxFont(wxFontInfo(16)));
    m_external_circulate_switch = new SwitchButton(external_circulate_panel);
    m_external_circulate_switch->SetBackgroundColour(*wxWHITE);
    m_external_circulate_switch->Bind(wxEVT_TOGGLEBUTTON, &StartFilter::onAirFilterToggled, this);

    bSizer_external_circulate_hor->AddSpacer(FromDIP(17));
    bSizer_external_circulate_hor->Add(m_staticText_external_circulate, 0, wxLEFT, 0);
    bSizer_external_circulate_hor->AddSpacer(FromDIP(7));
    bSizer_external_circulate_hor->Add(m_external_circulate_switch, 0, wxLEFT, 0);

    external_circulate_panel->SetSizer(bSizer_external_circulate_hor);
    external_circulate_panel->Layout();
    bSizer_external_circulate_hor->Fit(external_circulate_panel);

    sizer->Add(m_panel_filtering_title, 0, wxALL, 0);
    sizer->AddSpacer(FromDIP(12));
    sizer->Add(internal_circulate_panel, 0, wxLEFT, 0);
    sizer->AddSpacer(FromDIP(12));
    sizer->Add(external_circulate_panel, 0, wxLEFT, 0);
#ifdef __WIN32__
    sizer->AddSpacer(FromDIP(172));
#else if __APPLE__
    sizer->AddSpacer(FromDIP(181));
#endif
    parent->SetSizer(sizer);
    parent->Layout();
    parent->Fit();  
}

void StartFilter::setAirFilterState(bool internalOpen, bool externalOpen)
{ 
    m_internal_circulate_switch->SetValue(internalOpen);
    m_external_circulate_switch->SetValue(externalOpen);
}

void StartFilter::setCurId(int curId) 
{
    if (curId < 0) {
        return;
    }
    m_cur_id = curId; 
}

void StartFilter::onAirFilterToggled(wxCommandEvent &event)
{
    event.Skip();
    SwitchButton *click_btn   = dynamic_cast<SwitchButton *>(event.GetEventObject());
    std::string inter_state = CLOSE;
    std::string exter_state = CLOSE;

    if (m_internal_circulate_switch) {
        inter_state = m_internal_circulate_switch->GetValue() ? OPEN : CLOSE;
    }

    if (m_external_circulate_switch) {
        exter_state = m_external_circulate_switch->GetValue() ? OPEN : CLOSE;
    }

    if (m_internal_circulate_switch->GetValue() && m_external_circulate_switch->GetValue()) {
        if (click_btn == m_internal_circulate_switch) {
            m_external_circulate_switch->SetValue(false);
            exter_state = CLOSE;
        } else if (click_btn == m_external_circulate_switch) {
            m_internal_circulate_switch->SetValue(false);
            inter_state = CLOSE;
        }
    }
    ComAirFilterCtrl *filterCtrl = new ComAirFilterCtrl(inter_state, exter_state);
    if (m_cur_id >= 0) {
        Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, filterCtrl);
    }
}

wxDEFINE_EVENT(EVT_MODIFY_TEMP_CLICKED, wxCommandEvent);
wxDEFINE_EVENT(EVT_MODIFY_TEMP_CANCEL_CLICKED, wxCommandEvent);
ModifyTemp::ModifyTemp(wxWindow *parent) 
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
    SetBackgroundColour(*wxWHITE);
    create_panel(this);
}

void ModifyTemp::create_panel(wxWindow *parent) 
{
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *bSizer_temp_title = new wxBoxSizer(wxHORIZONTAL);

    auto m_panel_temp_title = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(36)), wxTAB_TRAVERSAL);
    m_panel_temp_title->SetBackgroundColour(wxColour(248, 248, 248));

    // 温度标题
    m_staticText_title = new wxStaticText(m_panel_temp_title, wxID_ANY, TEMPERATURE);
    m_staticText_title->SetForegroundColour(wxColour(51, 51, 51));

    bSizer_temp_title->Add(m_staticText_title, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(17));
    bSizer_temp_title->Add(0, 0, 1, wxEXPAND, 0);
    m_panel_temp_title->SetSizer(bSizer_temp_title);
    m_panel_temp_title->Layout();
    bSizer_temp_title->Fit(m_panel_temp_title);

    // 确认、取消按钮
    //垂直布局、水平布局
    wxBoxSizer *bSizer_operate_hor = new wxBoxSizer(wxHORIZONTAL);
    wxPanel *operate_panel = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(109)), wxTAB_TRAVERSAL);
    m_cancel_btn = new FFButton(operate_panel, wxID_ANY, TEMP_CANCEL);
    m_cancel_btn->SetMinSize(wxSize(FromDIP(64), FromDIP(32)));
    m_cancel_btn->SetFontHoverColor(wxColour(255, 255, 255));
    m_cancel_btn->SetBGHoverColor(wxColour(127, 127, 127));
    m_cancel_btn->SetBorderHoverColor(wxColour(127, 127, 127));

    m_cancel_btn->SetFontPressColor(wxColour(255, 255, 255));
    m_cancel_btn->SetBGPressColor(wxColour(31, 31, 31));
    m_cancel_btn->SetBorderPressColor(wxColour(31, 31, 31));

    m_cancel_btn->SetFontColor(wxColour(255, 255, 255));
    m_cancel_btn->SetBorderColor(wxColour(75, 75, 75));
    m_cancel_btn->SetBGColor(wxColour(75, 75, 75));
    m_cancel_btn->Bind(wxEVT_LEFT_DOWN, [this, operate_panel](wxMouseEvent &event) {
        event.Skip();
        wxCommandEvent ev(EVT_MODIFY_TEMP_CANCEL_CLICKED, GetId());
        ev.SetEventObject(this);
        wxPostEvent(this, ev);
    });

    bSizer_operate_hor->AddStretchSpacer();
    bSizer_operate_hor->Add(m_cancel_btn, 0, wxALIGN_CENTER, 0);
    bSizer_operate_hor->AddSpacer(FromDIP(19));

    m_confirm_btn = new FFButton(operate_panel, wxID_ANY, TEMP_CONFIRM);
    m_confirm_btn->SetMinSize(wxSize(FromDIP(64), FromDIP(32)));
    m_confirm_btn->SetFontHoverColor(wxColour(255, 255, 255));
    m_confirm_btn->SetBGHoverColor(wxColour(149, 197, 255));
    m_confirm_btn->SetBorderHoverColor(wxColour(149, 197, 255));

    m_confirm_btn->SetFontPressColor(wxColour(255, 255, 255));
    m_confirm_btn->SetBGPressColor(wxColour(17, 111, 223));
    m_confirm_btn->SetBorderPressColor(wxColour(17, 111, 223));

    m_confirm_btn->SetFontColor(wxColour(255, 255, 255));
    m_confirm_btn->SetBorderColor(wxColour(50, 141, 251));
    m_confirm_btn->SetBGColor(wxColour(50, 141, 251));
    m_confirm_btn->Bind(wxEVT_LEFT_DOWN, [this, operate_panel](wxMouseEvent &event) {
        event.Skip();
        wxCommandEvent ev(EVT_MODIFY_TEMP_CLICKED, GetId());
        ev.SetEventObject(this);
        wxPostEvent(this, ev);
    });

    bSizer_operate_hor->Add(m_confirm_btn, 0, wxALIGN_CENTER, 0);
    bSizer_operate_hor->AddStretchSpacer();

    operate_panel->SetSizer(bSizer_operate_hor);
    operate_panel->Layout();
    bSizer_operate_hor->Fit(operate_panel);

    sizer->Add(m_panel_temp_title, 0, wxEXPAND | wxALL, 0);
    sizer->AddSpacer(FromDIP(12));
    sizer->Add(operate_panel, 0, wxEXPAND | wxALL, 0);
    sizer->AddSpacer(FromDIP(87));

    parent->SetSizer(sizer);
    parent->Layout();
    parent->Fit();
}

void ModifyTemp::setLabel(wxString labelText) 
{
    m_staticText_title->SetLabel(labelText); 
}

DeviceDetail::DeviceDetail(wxWindow* parent)
    : wxPanel(parent, wxID_ANY,wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
    SetBackgroundColour(*wxWHITE);
    create_panel(this);
}

void DeviceDetail::setCurId(int curId) 
{ 
    if (curId < 0) {
        return;
    }
    m_cur_id = curId;
}

void DeviceDetail::create_panel(wxWindow* parent)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer *bSizer_confirm_row = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_confirm_row = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    auto confirm_push_btn = new FFPushButton(m_panel_confirm_row, wxID_ANY, "push_button_confirm_normal", "push_button_confirm_hover","push_button_confirm_press", "push_button_confirm_normal");
    confirm_push_btn->SetBackgroundColour(wxColour(255, 255, 255));
    confirm_push_btn->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &event) {
        event.Skip();
        double speed;
        double z_axis;
        double nozzle_fan;
        double cooling_fan;
        switchPage();
        if (m_device_speed) {
            wxString str_speed = m_device_speed->getTextValue();
            str_speed.ToDouble(&speed);
            double ss = speed;
        }
        if (m_device_z_axis) {
            m_device_z_axis->getTextValue().ToDouble(&z_axis);
        }
        if (m_device_nozzle_fan) {
            m_device_nozzle_fan->getTextValue().ToDouble(&nozzle_fan);
        }
        if (m_device_cooling_fan) {
            m_device_cooling_fan->getTextValue().ToDouble(&cooling_fan);
        }
        ComPrintCtrl *printCtrl = new ComPrintCtrl(z_axis, speed, nozzle_fan, 0, cooling_fan);
        if (m_cur_id >= 0) {
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, printCtrl);
        }
    });

    bSizer_confirm_row->AddSpacer(FromDIP(410));
    bSizer_confirm_row->Add(confirm_push_btn, 0, wxTOP, FromDIP(17));
    //bSizer_confirm_row->AddSpacer(FromDIP(25));

    m_panel_confirm_row->SetSizer(bSizer_confirm_row);
    m_panel_confirm_row->Layout();
    bSizer_confirm_row->Fit(m_panel_confirm_row);

    sizer->Add(m_panel_confirm_row, 0, wxALL, 0);

//
    auto m_panel_separotor10 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor10->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_separotor10->SetMinSize(wxSize(-1,FromDIP(18)));

    sizer->Add(m_panel_separotor10);
//
    wxBoxSizer *bSizer_h = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_separotor0 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor0->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_separotor0->SetMinSize(wxSize(FromDIP(28),-1));

    bSizer_h->Add(m_panel_separotor0);

    wxBoxSizer *bSizer_first_row = new wxBoxSizer(wxVERTICAL);
    auto m_panel_first_row = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), wxTAB_TRAVERSAL);
    m_device_material = new IconText(m_panel_first_row, wxString("device_material"), 20, wxString("PLA-12345678901234567890"), 12);
    bSizer_first_row->Add(m_device_material, 0, wxALL, 0);
    bSizer_first_row->AddSpacer(FromDIP(30));

    m_device_initial_speed = new IconText(m_panel_first_row, wxString("device_initial_speed"), 20, wxString("1000000mm/s"), 12);
    bSizer_first_row->Add(m_device_initial_speed, 0,wxALL, 0);
    bSizer_first_row->AddSpacer(FromDIP(30));

    m_device_speed = new IconBottonText(m_panel_first_row, wxString("device_speed"), 20, wxString("90"), 12);
    m_device_speed->setLimit(50, 150);
    m_device_speed->setAdjustValue(10);
    bSizer_first_row->Add(m_device_speed, 0,wxALL, 0);
    bSizer_first_row->AddSpacer(FromDIP(30));

    m_device_z_axis = new IconBottonText(m_panel_first_row, wxString("device_z_axis"), 20, wxString("0.002"), 12,
                                            wxString("device_z_dec"), wxString("push_button_arrow_dec_normal"));
    m_device_z_axis->setLimit(-5, 5);
    m_device_z_axis->setAdjustValue(0.025);
    m_device_z_axis->setPoint(3);
    bSizer_first_row->Add(m_device_z_axis, 0, wxALL, 0);
    bSizer_first_row->AddStretchSpacer();

    m_panel_first_row->SetSizer(bSizer_first_row);
    m_panel_first_row->Layout();
    bSizer_first_row->Fit(m_panel_first_row);

    bSizer_h->Add(m_panel_first_row, 0, wxALL, 0);
    //bSizer_h->AddStretchSpacer();
    //bSizer_h->AddSpacer(FromDIP(20));

//
    wxBoxSizer *bSizer_second_row  = new wxBoxSizer(wxVERTICAL);
    auto m_panel_second_row = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), wxTAB_TRAVERSAL);

    m_device_layer = new IconText(m_panel_second_row, wxString("device_layer"), 20, wxString("1000000/15000000"), 12);
    bSizer_second_row->Add(m_device_layer, 0, wxALL, 0);
    bSizer_second_row->AddSpacer(FromDIP(30));

    m_device_fill_rate = new IconText(m_panel_second_row, wxString("device_fill_rate"), 20, wxString("200%"), 12);
    bSizer_second_row->Add(m_device_fill_rate, 0, wxALL, 0);
    bSizer_second_row->AddSpacer(FromDIP(30));

    m_device_nozzle_fan = new IconBottonText(m_panel_second_row, wxString("device_nozzle_fan"), 20, wxString("50"), 12);
    m_device_nozzle_fan->setLimit(0, 100);
    m_device_nozzle_fan->setAdjustValue(10);
    bSizer_second_row->Add(m_device_nozzle_fan, 0, wxALL, 0);
    bSizer_second_row->AddSpacer(FromDIP(30));

    m_device_cooling_fan = new IconBottonText(m_panel_second_row, wxString("device_cooling_fan"), 20, wxString("100"), 12);
    m_device_cooling_fan->setLimit(0, 100);
    m_device_cooling_fan->setAdjustValue(10);
    bSizer_second_row->Add(m_device_cooling_fan, 0, wxALL, 0);
    bSizer_second_row->AddStretchSpacer();

    m_panel_second_row->SetSizer(bSizer_second_row);
    m_panel_second_row->Layout();
    bSizer_second_row->Fit(m_panel_second_row);

    bSizer_h->Add(m_panel_second_row, 0, wxALL, 0);

    //auto m_panel_separotor1 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    //m_panel_separotor1->SetBackgroundColour(wxColour(255, 255, 255));
    //m_panel_separotor1->SetMinSize(wxSize(FromDIP(25), -1));

    //bSizer_h->Add(m_panel_separotor1);

    sizer->Add(bSizer_h, 0, wxALL, 0);
#ifdef __WIN32__
    sizer->AddSpacer(FromDIP(44));
#else if __APPLE__
    sizer->AddSpacer(FromDIP(46));
#endif

    parent->SetSizer(sizer);
    parent->Layout();
    parent->Fit(); 
}

void DeviceDetail::switchPage() 
{
    if (m_device_speed) {
        m_device_speed->checkValue();
    }
    if (m_device_z_axis) {
        m_device_z_axis->checkValue();
    }
    if (m_device_nozzle_fan) {
        m_device_nozzle_fan->checkValue();
    }
    if (m_device_cooling_fan) {
        m_device_cooling_fan->checkValue();
    }
}

void DeviceDetail::setMaterialName(wxString materialName) 
{
    m_device_material->setText(materialName);
}

void DeviceDetail::setInitialSpeed(double initialSpeed)
{ 
    auto aInitialSpeed = static_cast<int>(initialSpeed);
    m_device_initial_speed->setText(wxString::Format("%d mm/s", aInitialSpeed));
}

void DeviceDetail::setSpeed(double speed) 
{ 
    auto aspeed = static_cast<int>(speed);
    m_device_speed->setText(wxString::Format("%d", aspeed));
    m_device_speed->setCurValue(aspeed);
}

void DeviceDetail::setZAxis(double value) 
{
    auto aValue = wxString::Format("%.3f", value);
    m_device_z_axis->setText(aValue);
    m_device_z_axis->setCurValue(value);
}

void DeviceDetail::setLayer(int printLayer, int targetLayer)
{
    wxString str_printerLayer = wxString::Format("%d", printLayer);
    wxString str_targetLayer  = wxString::Format("%d", targetLayer);
    str_printerLayer.Append("/");
    str_printerLayer.Append(str_targetLayer);
    m_device_layer->setText(str_printerLayer);
}

void DeviceDetail::setFillRate(double fillRate) 
{
    auto aFillRate = static_cast<int>(fillRate);
    m_device_fill_rate->setText(wxString::Format("%d %%", aFillRate));
}

void DeviceDetail::setCoolingFanSpeed(double fanSpeed) 
{ 
    auto aFanSpeed = static_cast<int>(fanSpeed);
    m_device_nozzle_fan->setText(wxString::Format("%d", aFanSpeed));
    m_device_nozzle_fan->setCurValue(aFanSpeed);
}

void DeviceDetail::setCoolingFanShow(bool show) { m_device_cooling_fan->Show(show); }

void DeviceDetail::setChamberFanSpeed(double fanSpeed) 
{ 
    auto aFanSpeed = static_cast<int>(fanSpeed);
    m_device_cooling_fan->setText(wxString::Format("%d", aFanSpeed));
    m_device_cooling_fan->setCurValue(aFanSpeed);
}

G3UDetail::G3UDetail(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
    SetBackgroundColour(*wxWHITE);
    create_panel(this);
}

void G3UDetail::setCurId(int curId) 
{
    if (curId < 0) {
        return;
    }
    m_cur_id = curId;
}

void G3UDetail::create_panel(wxWindow* parent) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* bSizer_confirm_row  = new wxBoxSizer(wxHORIZONTAL);
    auto        m_panel_confirm_row = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    auto confirm_push_btn = new FFPushButton(m_panel_confirm_row, wxID_ANY, "push_button_confirm_normal", "push_button_confirm_hover",
                                             "push_button_confirm_press", "push_button_confirm_normal");
    confirm_push_btn->SetBackgroundColour(wxColour(255, 255, 255));
    confirm_push_btn->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& event) {
        event.Skip();
        double speed;
        double z_axis;
        double right_nozzle_fan;
        double left_nozzle_fan;
        double cooling_fan;
        switchPage();
        if (m_device_speed) {
            wxString str_speed = m_device_speed->getTextValue();
            str_speed.ToDouble(&speed);
            double ss = speed;
        }
        if (m_device_z_axis) {
            m_device_z_axis->getTextValue().ToDouble(&z_axis);
        }
        if (m_device_right_nozzle_fan) {
            m_device_right_nozzle_fan->getTextValue().ToDouble(&right_nozzle_fan);
        }
        if (m_device_left_nozzle_fan) {
            m_device_left_nozzle_fan->getTextValue().ToDouble(&left_nozzle_fan);
        }
        if (m_device_cooling_fan) {
            m_device_cooling_fan->getTextValue().ToDouble(&cooling_fan);
        }
        ComPrintCtrl* printCtrl = new ComPrintCtrl(z_axis, speed, right_nozzle_fan, left_nozzle_fan, cooling_fan);
        if (m_cur_id >= 0) {
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, printCtrl);
        }
    });

    bSizer_confirm_row->AddSpacer(FromDIP(410));
    bSizer_confirm_row->Add(confirm_push_btn, 0, wxTOP, FromDIP(10));
    // bSizer_confirm_row->AddSpacer(FromDIP(25));

    m_panel_confirm_row->SetSizer(bSizer_confirm_row);
    m_panel_confirm_row->Layout();
    bSizer_confirm_row->Fit(m_panel_confirm_row);

    sizer->Add(m_panel_confirm_row, 0, wxALL, 0);

    //
    auto m_panel_separotor10 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor10->SetBackgroundColour(wxColour(255, 255, 255));
#ifdef __WIN32__
    m_panel_separotor10->SetMinSize(wxSize(-1, FromDIP(10)));
#else if __APPLE__
    m_panel_separotor10->SetMinSize(wxSize(-1, FromDIP(10)));
#endif


    sizer->Add(m_panel_separotor10);
    //
    wxBoxSizer* bSizer_h           = new wxBoxSizer(wxHORIZONTAL);
    auto        m_panel_separotor0 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor0->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_separotor0->SetMinSize(wxSize(FromDIP(28), -1));

    bSizer_h->Add(m_panel_separotor0);

    wxBoxSizer* bSizer_first_row  = new wxBoxSizer(wxVERTICAL);
    auto        m_panel_first_row = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), wxTAB_TRAVERSAL);
    m_device_material = new IconText(m_panel_first_row, wxString("device_material"), 20, wxString("PLA-12345678901234567890"), 12);
    bSizer_first_row->Add(m_device_material, 0, wxALL, 0);

    wxBoxSizer* b1Sizer_h = new wxBoxSizer(wxHORIZONTAL);
    b1Sizer_h->AddSpacer(FromDIP(30));  
    m_rightMaterial = new wxStaticText(m_panel_first_row, wxID_ANY, _L(""), wxDefaultPosition, wxDefaultSize);
    
    b1Sizer_h->Add(m_rightMaterial);
    bSizer_first_row->Add(b1Sizer_h);
#ifdef __WIN32__
    bSizer_first_row->AddSpacer(FromDIP(14));
#else if __APPLE__
    bSizer_first_row->AddSpacer(FromDIP(12));
#endif

    m_device_initial_speed = new IconText(m_panel_first_row, wxString("device_initial_speed"), 20, wxString("1000000mm/s"), 12);
    bSizer_first_row->Add(m_device_initial_speed, 0, wxALL, 0);
    bSizer_first_row->AddSpacer(FromDIP(18));

    m_device_speed = new IconBottonText(m_panel_first_row, wxString("device_speed"), 20, wxString("90"), 12);
    m_device_speed->setLimit(50, 150);
    m_device_speed->setAdjustValue(10);
    bSizer_first_row->Add(m_device_speed, 0, wxALL, 0);
    bSizer_first_row->AddSpacer(FromDIP(18));

    m_device_z_axis = new IconBottonText(m_panel_first_row, wxString("device_z_axis"), 20, wxString("0.002"), 12, wxString("device_z_dec"),
                                         wxString("push_button_arrow_dec_normal"));
    m_device_z_axis->setLimit(-5, 5);
    m_device_z_axis->setAdjustValue(0.025);
    m_device_z_axis->setPoint(3);
    bSizer_first_row->Add(m_device_z_axis, 0, wxALL, 0);
    bSizer_first_row->AddSpacer(FromDIP(18));

    m_device_cooling_fan = new IconBottonText(m_panel_first_row, wxString("device_cooling_fan"), 20, wxString("100"), 12);
    m_device_cooling_fan->setLimit(0, 100);
    m_device_cooling_fan->setAdjustValue(10);
    bSizer_first_row->Add(m_device_cooling_fan, 0, wxALL, 0);

    bSizer_first_row->AddStretchSpacer();

    m_panel_first_row->SetSizer(bSizer_first_row);
    m_panel_first_row->Layout();
    bSizer_first_row->Fit(m_panel_first_row);

    bSizer_h->Add(m_panel_first_row, 0, wxALL, 0);

    //
    wxBoxSizer* bSizer_second_row  = new wxBoxSizer(wxVERTICAL);
    auto        m_panel_second_row = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), wxTAB_TRAVERSAL);

    m_device_layer = new IconText(m_panel_second_row, wxString("device_layer"), 20, wxString("1000000/15000000"), 12);
    bSizer_second_row->Add(m_device_layer, 0, wxALL, 0);
    bSizer_second_row->AddSpacer(FromDIP(18));

    bSizer_second_row->AddSpacer(FromDIP(14));

    m_device_fill_rate = new IconText(m_panel_second_row, wxString("device_fill_rate"), 20, wxString("200%"), 12);
    bSizer_second_row->Add(m_device_fill_rate, 0, wxALL, 0);
    bSizer_second_row->AddSpacer(FromDIP(18));

    m_device_right_nozzle_fan = new IconBottonText(m_panel_second_row, wxString("device_right_nozzle_fan"), 20, wxString("50"), 12);
    m_device_right_nozzle_fan->setLimit(0, 100);
    m_device_right_nozzle_fan->setAdjustValue(10);
    bSizer_second_row->Add(m_device_right_nozzle_fan, 0, wxALL, 0);
    bSizer_second_row->AddSpacer(FromDIP(18));

    m_device_left_nozzle_fan = new IconBottonText(m_panel_second_row, wxString("device_left_nozzle_fan"), 20, wxString("100"), 12);
    m_device_left_nozzle_fan->setLimit(0, 100);
    m_device_left_nozzle_fan->setAdjustValue(10);
    bSizer_second_row->Add(m_device_left_nozzle_fan, 0, wxALL, 0);
    bSizer_second_row->AddStretchSpacer();

    m_panel_second_row->SetSizer(bSizer_second_row);
    m_panel_second_row->Layout();
    bSizer_second_row->Fit(m_panel_second_row);

    bSizer_h->Add(m_panel_second_row, 0, wxALL, 0);

    sizer->Add(bSizer_h, 0, wxALL, 0);
#ifdef __WIN32__
    sizer->AddSpacer(FromDIP(43));
#else if __APPLE__
    sizer->AddSpacer(FromDIP(46));
#endif

    parent->SetSizer(sizer);
    parent->Layout();
    parent->Fit();
}

void G3UDetail::switchPage()
{
    if (m_device_speed) {
        m_device_speed->checkValue();
    }
    if (m_device_z_axis) {
        m_device_z_axis->checkValue();
    }
    if (m_device_right_nozzle_fan) {
        m_device_right_nozzle_fan->checkValue();
    }
    if (m_device_left_nozzle_fan) {
        m_device_left_nozzle_fan->checkValue();
    }
    if (m_device_cooling_fan) {
        m_device_cooling_fan->checkValue();
    }
}

void G3UDetail::setMaterialName(wxString materialName) 
{ 
    m_device_material->setText(materialName); 
}

void G3UDetail::setRightMaterialName(wxString materialName) 
{
    m_rightMaterial->SetForegroundColour(wxColour(50, 141, 251));
    m_rightMaterial->SetLabelText(materialName);
}

void G3UDetail::setInitialSpeed(double initialSpeed)
{
    auto aInitialSpeed = static_cast<int>(initialSpeed);
    m_device_initial_speed->setText(wxString::Format("%d mm/s", aInitialSpeed));
}

void G3UDetail::setSpeed(double speed)
{
    auto aspeed = static_cast<int>(speed);
    m_device_speed->setText(wxString::Format("%d", aspeed));
    m_device_speed->setCurValue(aspeed);
}

void G3UDetail::setZAxis(double value)
{
    auto aValue = wxString::Format("%.3f", value);
    m_device_z_axis->setText(aValue);
    m_device_z_axis->setCurValue(value);
}

void G3UDetail::setLayer(int printLayer, int targetLayer)
{
    wxString str_printerLayer = wxString::Format("%d", printLayer);
    wxString str_targetLayer  = wxString::Format("%d", targetLayer);
    str_printerLayer.Append("/");
    str_printerLayer.Append(str_targetLayer);
    m_device_layer->setText(str_printerLayer);
}

void G3UDetail::setFillRate(double fillRate)
{
    auto aFillRate = static_cast<int>(fillRate);
    m_device_fill_rate->setText(wxString::Format("%d %%", aFillRate));
}

void G3UDetail::setCoolingFanSpeed(double fanSpeed)
{
    auto aFanSpeed = static_cast<int>(fanSpeed);
    m_device_right_nozzle_fan->setText(wxString::Format("%d", aFanSpeed));
    m_device_right_nozzle_fan->setCurValue(aFanSpeed);
}

void G3UDetail::setLeftCoolingFanSpeed(double leftFanSpeed) 
{
    auto aFanSpeed = static_cast<int>(leftFanSpeed);
    m_device_left_nozzle_fan->setText(wxString::Format("%d", aFanSpeed));
    m_device_left_nozzle_fan->setCurValue(aFanSpeed);
}

void G3UDetail::setChamberFanSpeed(double fanSpeed)
{
    auto aFanSpeed = static_cast<int>(fanSpeed);
    m_device_cooling_fan->setText(wxString::Format("%d", aFanSpeed));
    m_device_cooling_fan->setCurValue(aFanSpeed);
}

wxDEFINE_EVENT(EVT_FILE_ITEM_CLICKED, wxCommandEvent);

FileItem::FileItem(wxWindow* parent, const FileData& data) 
    : wxPanel(parent), 
      m_data(data) 
{
    //create_panel(parent);
    int width = FromDIP(450), height = FromDIP(44);
    SetMinSize(wxSize(width, height));
    SetMaxSize(wxSize(width, height));
    SetSize(wxSize(width, height));
    Bind(wxEVT_ENTER_WINDOW, &FileItem::on_mouse_enter, this);
    Bind(wxEVT_LEAVE_WINDOW, &FileItem::on_mouse_leave, this);
    Bind(wxEVT_LEFT_UP, &FileItem::on_mouse_left_up, this);
    Bind(wxEVT_PAINT, &FileItem::OnPaint, this);
}

bool FileItem::IsPressed() const 
{
    return m_pressed;
}

void FileItem::SetPressed(bool pressed)
{
    m_pressed = pressed; 
}

void FileItem::create_panel(wxWindow* parent)
{
    int width = FromDIP(450), height = FromDIP(33);
    SetMinSize(wxSize(width, height));
    SetMaxSize(wxSize(width, height));
    SetSize(wxSize(width, height));

    m_iconPanel      = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_iconSizer      = new wxBoxSizer(wxVERTICAL);
    m_thumbnailPanel = new ThumbnailPanel(m_iconPanel);
    m_thumbnailPanel->SetSize(wxSize(height, height));
    m_thumbnailPanel->SetMinSize(wxSize(height, height));
    m_thumbnailPanel->SetMaxSize(wxSize(height, height));
    m_iconSizer->Add(m_thumbnailPanel, 0, wxEXPAND, 0);
    m_iconPanel->SetSizer(m_iconSizer);
    m_iconPanel->Layout();

    //set material pic
    // m_thumbnailPanel->set_thumbnail(iter->second);

    //fix file name tip
    m_fileLbl = new wxStaticText(this, wxID_ANY, _L("file name:"), wxDefaultPosition, wxDefaultSize);
    m_fileLbl->SetMinSize(wxSize(FromDIP(70), height));
    m_fileLbl->SetMaxSize(wxSize(FromDIP(70), height));
    m_fileLbl->SetSize(wxSize(FromDIP(70), height));
    m_fileLbl->SetForegroundColour(wxColor("#333333"));

    //file name
    m_nameLbl = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize);
    m_nameLbl->SetMinSize(wxSize(FromDIP(200), height));
    m_nameLbl->SetMaxSize(wxSize(FromDIP(200), height));
    m_nameLbl->SetSize(wxSize(FromDIP(200), height));
    m_nameLbl->SetForegroundColour(wxColor("#333333"));

    wxString elide_str = FFUtils::elideString(m_nameLbl, m_data.wxName, FromDIP(200));
    m_nameLbl->SetLabel(elide_str);
    //m_nameLbl->Wrap(name_width);
    //m_nameLbl->Fit();

    m_mainSizer = new wxBoxSizer(wxHORIZONTAL);
    // m_mainSizer->AddSpacer(FromDIP(5));
    m_mainSizer->Add(m_iconPanel, 0, wxALIGN_CENTER_VERTICAL);
    m_mainSizer->Add(m_fileLbl, 0, wxALIGN_CENTER_VERTICAL);
    m_mainSizer->Add(m_nameLbl ,1, wxALIGN_CENTER_VERTICAL);

    SetSizer(m_mainSizer);

    Layout();
    Fit();
}

wxImage FileItem::getImageByType(const std::string& type) 
{
    std::string imageName;
    if (type.compare("3mf") == 0) {
        imageName = "flielist_3MF" ;
    } else if (type.compare("gcode") == 0) {
        imageName = "filelist_gcode";
    } else if (type.compare("g") == 0) {
        imageName = "filelist_g";
    } else if (type.compare("gx") == 0) {
        imageName = "filelist_gx";
    }
    return wxImage(imageName);
}

std::string FileItem::getImageNameByType(const std::string& type) 
{
    std::string imageName;
    if (type.compare("3mf") == 0) {
        imageName = "flielist_3MF";
    } else if (type.compare("gcode") == 0) {
        imageName = "filelist_gcode";
    } else if (type.compare("g") == 0) {
        imageName = "filelist_g";
    } else if (type.compare("gx") == 0) {
        imageName = "filelist_gx";
    } else {
        imageName = "flielist_3MF";
    }
    return imageName;
}

void FileItem::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    doRender(dc);
}

void FileItem::render(wxDC& dc)
{
#ifdef __WXMSW__
    wxSize     size = GetSize();
    wxMemoryDC memdc;
    wxBitmap   bmp(size.x, size.y);
    memdc.SelectObject(bmp);
    memdc.Blit({0, 0}, size, &dc, {0, 0});

    {
        wxGCDC dc2(memdc);
        doRender(dc2);
    }

    memdc.SelectObject(wxNullBitmap);
    dc.DrawBitmap(bmp, 0, 0);
#else
    doRender(dc);
#endif
}

void FileItem::doRender(wxDC& dc)
{
    auto   left = 20;
    wxSize size = GetSize();
    dc.SetPen(*wxTRANSPARENT_PEN);

    if (m_pressed) {
        dc.SetBrush(m_bg_color);
        dc.SetPen(m_border_presse_color);
        dc.DrawRectangle(0, 0, size.x, size.y);
    }

    if (m_hovered) {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(m_border_hover_color, 1, wxPENSTYLE_SOLID));
        dc.DrawRectangle(0, 0, size.x, size.y);
    }

    if (m_data.scaledImage.IsOk()) {
        wxBitmap bitmap = m_data.scaledImage;
        dc.DrawBitmap(bitmap, wxPoint(left, (size.y - m_data.scaledImage.GetHeight()) / 2));
        left += m_data.scaledImage.GetWidth() + 8;
    } else {
        std::string name = m_data.gcodeData.fileName;
        std::string suffix = name.substr(name.find_last_of(".") + 1);
        std::string imageName = getImageNameByType(suffix);

        ScalableBitmap dwbitmap = ScalableBitmap(this, imageName, 43);
        wxImage defaultImage = dwbitmap.bmp().ConvertToImage();
        defaultImage.Rescale(FILELIST_PIC_WIDTH, FILELIST_PIC_HEIGHT);
        wxBitmap bitmap = defaultImage;
        dc.DrawBitmap(bitmap, wxPoint(left, (size.y - defaultImage.GetHeight()) / 2));
        left += dwbitmap.GetBmpSize().x + 8;
    }
    
    dc.SetFont(Label::Body_13);
    dc.SetBackgroundMode(wxTRANSPARENT);
    dc.SetTextForeground(StateColor::darkModeColorFor(wxColour(38, 46, 48)));

    auto sizet = dc.GetTextExtent(m_data.wxName);
    wxString elide_str = FFUtils::elideString(this, m_data.wxName, FromDIP(200));
    dc.DrawText(elide_str, wxPoint(left, (size.y - sizet.y) / 2));
}

void FileItem::on_mouse_enter(wxMouseEvent& evt)
{
    m_hovered = true;
    Refresh();
}

void FileItem::on_mouse_leave(wxMouseEvent& evt)
{
    m_hovered = false;
    Refresh();
}

void FileItem::on_mouse_left_up(wxMouseEvent& evt) 
{
    wxCommandEvent event(EVT_FILE_ITEM_CLICKED, GetId());
    event.SetEventObject(this);
    wxPostEvent(this, event);

    m_pressed = true;
    Refresh();
}

SingleDeviceState::SingleDeviceState(wxWindow* parent, wxWindowID id, const wxPoint& pos, 
        const wxSize& size, long style, const wxString& name)
        : wxScrolledWindow(parent, id, pos, size, wxHSCROLL | wxVSCROLL)
        , m_cur_printing_ctrl(0)
        , m_download_tool(5, 15000)
        , m_download_title_image_task_id(FFDownloadTool::InvalidTaskId)
{
    this->SetScrollRate(30, 30);
    this->SetBackgroundColour(wxColour(240, 240, 240));
    setupLayout();
    connectEvent();
    reInit();
}

SingleDeviceState::~SingleDeviceState()
{
    m_download_tool.wait(true);
}

void SingleDeviceState::setCurId(int curId)
{ 
    if (curId < 0) {
        return;
    }
    if (curId != m_cur_id) {
        reInitMaterialPic();
        clearFileList();
        m_curId_first_Click_fileList = true;
        m_status_check_message_show_time = 0;
        m_status_check_error_code.clear();
    } 
    {
        if (m_idle_tempMixDevice && !m_idle_tempMixDevice->IsShown()) {
            m_panel_print_btn->Hide();
            m_scrolledWindow->Hide();
            m_FileList_split_line->Hide();
            m_timeLapseVideoPnl->Hide();

            m_printBtn->Enable(false);
            if (m_curSelectedFileItem) {
                m_curSelectedFileItem->SetPressed(false);
                m_curSelectedFileItem = nullptr;
            }
            m_idle_tempMixDevice->Show();
        }
    }
    m_cur_id = curId;
    m_busy_device_detial->setCurId(curId);
    m_busy_G3U_detail->setCurId(curId);
    m_busy_circula_filter->setCurId(curId);
    m_idle_tempMixDevice->setCurId(curId);
    m_timeLapseVideoPnl->setComId(curId);
    reInitProductState();
    m_idle_tempMixDevice->reInitProductState();

    //query device data by id
    bool  valid = false;
    const com_dev_data_t &data  = MultiComMgr::inst()->devData(m_cur_id, &valid);
    if (!valid) {
        setPageOffline();
        return;
    }
    unsigned short curr_pid = 0;
    if (data.connectMode == COM_CONNECT_LAN) {
        m_cur_serial_number = data.lanDevInfo.serialNumber;
        curr_pid            = data.lanDevInfo.pid;
        m_fileListbutton->SetMinSize((wxSize(FromDIP(450), FromDIP(45))));
        m_timeLapseVideoBtn->Show(false);
    } else if (data.connectMode == COM_CONNECT_WAN) {
        m_cur_serial_number = data.wanDevInfo.serialNumber;
        curr_pid            = data.devDetail->pid;
        m_fileListbutton->SetMinSize((wxSize(FromDIP(225), FromDIP(45))));
        m_timeLapseVideoBtn->Show(true);
    }

    // 根据机型判断是否支持四色打印，并设置currID
    std::string modelId = FFUtils::getPrinterModelId(curr_pid);
    bool isPrinterSupportAms = FFUtils::isPrinterSupportAms(modelId);
    bool isPrinterSupportCoolingFan = FFUtils::isPrinterSupportCoolingFan(modelId);
    bool isPrinterSupportDeviceFilter = FFUtils::isPrinterSupportDeviceFilter(modelId);
    m_material_station->show_material_panel(modelId);
    m_busy_device_detial->setCoolingFanShow(isPrinterSupportCoolingFan);
    if (isPrinterSupportAms) {
        m_material_station->setCurId(m_cur_id);
    }
    m_filter_button->Enable(isPrinterSupportDeviceFilter);
    m_filter_button->SetIcon(isPrinterSupportDeviceFilter ? "device_filter" : "device_filter_offline");

    changeMachineType(data.devDetail->pid);
    m_idle_tempMixDevice->changeMachineType(data.devDetail->pid);
    reInitPage();
    onDevStateChanged(data.devDetail->status, data);
    fillValue(data);
    Layout();
    reInitData();
}

void SingleDeviceState::modifyVideoPlayerAddress(const std::string &urlAddress)
{
    std::string cur_language = getCurLanguage();
    std::string jsonStr = R"({"command": "modify_rtsp_player_address","address": "http://115.231.29.48:1370/ffspace/SNMMOC98989898.m3u8","sequence_id": "10001"})";
    // 将JSON字符串解析为JSON对象
    json jsonObj = json::parse(jsonStr);
    if (!urlAddress.empty()) {
        jsonObj["address"] = urlAddress;
    } else {
        return;
    }
    jsonObj["language"] = cur_language;
    // 将JSON对象转换为字符串
    std::string newJsonStr = jsonObj.dump();
    wxString strJS = wxString::Format("window.postMessage(%s)", wxString::FromUTF8(newJsonStr));
    if (m_browser) {
        WebView::RunScript(m_browser, strJS);
    }
}

void SingleDeviceState::notifyWebDevOffline() 
{
    std::string cur_language = getCurLanguage();
    std::string jsonStr = R"({"command" : "close_rtsp", "sequence_id" : "10001"})";
    json jsonObj = json::parse(jsonStr);
    jsonObj["language"] = cur_language;
    std::string newJsonStr = jsonObj.dump();
    wxString strJS = wxString::Format("window.postMessage(%s)", wxString::FromUTF8(newJsonStr));
    if (m_browser) {
        WebView::RunScript(m_browser, strJS);
    }
}

void SingleDeviceState::reInit() 
{
    reInitData();
    reInitUI();
}

void SingleDeviceState::reInitData() 
{ 
    m_last_speed               = 0.00001;
    m_last_z_axis_compensation = 0.00001;
    m_last_cooling_fan_speed   = 0.00001;
    m_last_left_cooling_fan_speed = 0.00001;
    m_last_chamber_fan_speed   = 0.00001;
    m_right_target_temp        = 0.00001;
    m_plat_target_temp         = 0.00001;
    m_camera_stream_url.clear();
    m_file_pic_url.clear();
    m_file_pic_name.clear();
    m_cur_dev_state.clear();
    m_cur_print_file_name.clear();
}

void SingleDeviceState::reInitUI() 
{
    m_staticText_device_tip->SetLabel(_L("Offline"));
    m_staticText_device_tip->SetForegroundColour(wxColour("#999999"));
    if (m_panel_idle_text) {
        m_panel_idle_text->Hide();
        m_panel_print_btn->Hide();
        m_scrolledWindow->Hide();
        m_FileList_split_line->Hide();
        m_timeLapseVideoPnl->Hide();
        m_panel_separotor8->Hide();
        m_busyState_top_gap->Show();
        m_busyState_bottom_gap->Show();
    }
    m_staticText_device_info->Hide();
    m_clear_button->Hide();
    m_staticText_idle->SetLabel(_L("Device offline"));
    m_idle_tempMixDevice->modifyTemp("/", "/", "/");
    m_idle_tempMixDevice->setState(0);
    m_cur_printing_ctrl = 0;
    Layout();
}

void SingleDeviceState::reInitMaterialPic() 
{
   if (m_material_picture) {
     m_file_pic_url.clear();
     m_file_pic_name.clear();
     m_material_picture->SetImage(wxImage());
   }
}

void SingleDeviceState::reInitPage() 
{ 
    if (m_busy_temp_brn) {
        m_busy_temp_brn->Hide();
    }
    if (m_busy_device_detial) {
        m_busy_device_detial->Hide();
    }
    if (m_busy_G3U_detail) {
        m_busy_G3U_detail->Hide();
    }
    if (m_busy_circula_filter) {
        m_busy_circula_filter->Hide();
    }
    if (m_device_info_button) {
        m_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
    }
    if (m_filter_button) {
        m_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
    }
}

void SingleDeviceState::changeMachineType(unsigned short pid)
{
    switch (pid) {
    case 0x0023:  //"adventurer_5m"
    case 0x0024: //"adventurer_5m_pro"
    case 0x00BB: //"adventurer_a5"
        m_tempCtrl_top->SetNormalIcon("device_top_temperature");
        m_tempCtrl_top->SetIconNormal();
        m_tempCtrl_bottom->SetNormalIcon("device_bottom_temperature");
        m_tempCtrl_bottom->SetIconNormal();
        m_tempCtrl_mid->SetNormalIcon("device_mid_temperature");
        m_tempCtrl_mid->SetIconNormal();
        m_tempCtrl_mid->SetReadOnly(true);
        break;
    case 0x001F: //"guider_3_ultra"
        m_tempCtrl_top->SetNormalIcon("device_right_temperature");
        m_tempCtrl_top->SetIconNormal();
        m_tempCtrl_bottom->SetNormalIcon("device_left_temperature");
        m_tempCtrl_bottom->SetIconNormal();
        m_tempCtrl_mid->SetNormalIcon("device_bottom_temperature");
        m_tempCtrl_mid->SetIconNormal();
        m_tempCtrl_mid->SetReadOnly(false);
        break;
    }
}

void SingleDeviceState::setDevProductAuthority(const fnet_dev_product_t &data) 
{
    bool lightCtrl = data.lightCtrlState == 0 ? false : true;
    bool fanCtrl   = data.internalFanCtrlState == 0 ? false : true;
    if (!lightCtrl) {
        m_lamp_control_button->SetIcon("device_lamp_offline");
        m_lamp_control_button->Enable(false);
    }
    if (!fanCtrl) {
        m_filter_button->SetIcon("device_filter_offline");
        m_filter_button->Enable(false);
        if (m_busy_circula_filter) {
            m_busy_circula_filter->Hide();
        }
    }
}

void SingleDeviceState::setG3UProductAuthority(const fnet_dev_product_t& data) 
{
    bool lightCtrl = data.lightCtrlState == 0 ? false : true;
    if (!lightCtrl) {
        m_lamp_control_button->SetIcon("device_lamp_offline");
        m_lamp_control_button->Enable(false);
    }
}

void SingleDeviceState::reInitProductState()
{ 
    m_lamp_control_button->SetIcon("device_lamp_control");
    m_filter_button->SetIcon("device_filter");
    m_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_filter_button->SetBorderColor(wxColour(255, 255, 255));
    m_lamp_control_button->Enable(true); 
    m_filter_button->Enable(true);
}

std::string SingleDeviceState::getCurDevSerialNumber() 
{
    return m_cur_serial_number;
}

void SingleDeviceState::lostFocusmodifyTemp()
{
    double top_temp;
    double bottom_temp;
    double mid_temp;
    bool   bTop    = m_tempCtrl_top->GetTagTemp().ToDouble(&top_temp);
    bool   bBottom = m_tempCtrl_bottom->GetTagTemp().ToDouble(&bottom_temp);
    bool   bMid    = m_tempCtrl_mid->GetTagTemp().ToDouble(&mid_temp);
    switch (m_pid) {
    case 0x0023:
    case 0x0024: 
    case 0x00BB: {
        //"Flashforge-Adventurer-5M";
        //"Flashforge-Adventurer-A5";
        //"Flashforge-Adventurer-5M-Pro";
        if (!bTop || top_temp < 0) {
            m_tempCtrl_top->SetTagTemp(m_right_target_temp, true);
            top_temp = m_right_target_temp;
        }
        if (top_temp > 280) {
            top_temp = 280;
            m_tempCtrl_top->SetTagTemp(top_temp, true);
            m_right_target_temp = top_temp;
        } else if (top_temp < 0) {
            top_temp = 0;
            m_tempCtrl_top->SetTagTemp(top_temp, true);
            m_right_target_temp = top_temp;
        }
        if (!bBottom || bottom_temp < 0) {
            m_tempCtrl_bottom->SetTagTemp(m_plat_target_temp, true);
            bottom_temp = m_plat_target_temp;
        }
        if (bottom_temp > 110) {
            bottom_temp = 110;
            m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
            m_plat_target_temp = bottom_temp;
        } else if (bottom_temp < 0) {
            bottom_temp = 0;
            m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
            m_plat_target_temp = bottom_temp;
        }
        Slic3r::GUI::ComTempCtrl* tempCtrl = new Slic3r::GUI::ComTempCtrl(bottom_temp, top_temp, 0, mid_temp);
        Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, tempCtrl);
        break;
    }
    case 0x0025: 
    case 0x0027: {
        //"Flashforge-Guider4";
        //"Flashforge-Guider4-Pro"
        if (!bTop || top_temp < 0) {
            m_tempCtrl_top->SetTagTemp(m_right_target_temp, true);
            top_temp = m_right_target_temp;
        }
        if (top_temp > 320) {
            top_temp = 320;
            m_tempCtrl_top->SetTagTemp(top_temp, true);
            m_right_target_temp = top_temp;
        } else if (top_temp < 0) {
            top_temp = 0;
            m_tempCtrl_top->SetTagTemp(top_temp, true);
            m_right_target_temp = top_temp;
        }
        if (!bMid || mid_temp < 0) {
            m_tempCtrl_mid->SetTagTemp(m_chamber_target_temp, true);
            mid_temp = m_chamber_target_temp;
        }
        if (mid_temp > 60) {
            mid_temp = 60;
            m_tempCtrl_mid->SetTagTemp(mid_temp, true);
            m_chamber_target_temp = mid_temp;
        } else if (mid_temp < 0) {
            mid_temp = 0;
            m_tempCtrl_mid->SetTagTemp(mid_temp, true);
            m_chamber_target_temp = mid_temp;
        }
        if (!bBottom || bottom_temp < 0) {
            m_tempCtrl_bottom->SetTagTemp(m_plat_target_temp, true);
            bottom_temp = m_plat_target_temp;
        }
        if (bottom_temp > 120) {
            bottom_temp = 120;
            m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
            m_plat_target_temp = bottom_temp;
        } else if (bottom_temp < 0) {
            bottom_temp = 0;
            m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
            m_plat_target_temp = bottom_temp;
        }
        Slic3r::GUI::ComTempCtrl* tempCtrl = new Slic3r::GUI::ComTempCtrl(bottom_temp, top_temp, 0, mid_temp);
        Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, tempCtrl);
        break;
    }
    case 0x0026: {
        //"Flashforge-AD5X";
        if (!bTop || top_temp < 0) {
            m_tempCtrl_top->SetTagTemp(m_right_target_temp, true);
            top_temp = m_right_target_temp;
        }
        if (top_temp > 300) {
            top_temp = 300;
            m_tempCtrl_top->SetTagTemp(top_temp, true);
            m_right_target_temp = top_temp;
        } else if (top_temp < 0) {
            top_temp = 0;
            m_tempCtrl_top->SetTagTemp(top_temp, true);
            m_right_target_temp = top_temp;
        }
        if (!bBottom || bottom_temp < 0) {
            m_tempCtrl_bottom->SetTagTemp(m_plat_target_temp, true);
            bottom_temp = m_plat_target_temp;
        }
        if (bottom_temp > 110) {
            bottom_temp = 110;
            m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
            m_plat_target_temp = bottom_temp;
        } else if (bottom_temp < 0) {
            bottom_temp = 0;
            m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
            m_plat_target_temp = bottom_temp;
        }
        Slic3r::GUI::ComTempCtrl* tempCtrl = new Slic3r::GUI::ComTempCtrl(bottom_temp, top_temp, 0, mid_temp);
        Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, tempCtrl);
        break;
    }
    case 0x001F: {
        //"Flashforge-Guider-3-Ultra";
        // right
        if (!bTop || top_temp < 0) {
            m_tempCtrl_top->SetTagTemp(m_right_target_temp, true);
            top_temp = m_right_target_temp;
        }
        if (top_temp > 350) {
            top_temp = 350;
            m_tempCtrl_top->SetTagTemp(top_temp, true);
            m_right_target_temp = top_temp;
        } else if (top_temp < 0) {
            top_temp = 0;
            m_tempCtrl_top->SetTagTemp(top_temp, true);
            m_right_target_temp = top_temp;
        }
        // left
        if (!bBottom || bottom_temp < 0) {
            m_tempCtrl_bottom->SetTagTemp(m_plat_target_temp, true);
            bottom_temp = m_plat_target_temp;
        }
        if (bottom_temp > 350) {
            bottom_temp = 350;
            m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
            m_plat_target_temp = bottom_temp;
        } else if (bottom_temp < 0) {
            bottom_temp = 0;
            m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
            m_plat_target_temp = bottom_temp;
        }
        // bottom
        if (!bMid || mid_temp < 0) {
            m_tempCtrl_mid->SetTagTemp(m_chamber_target_temp, true);
            mid_temp = m_chamber_target_temp;
        }
        if (mid_temp > 120) {
            mid_temp = 120;
            m_tempCtrl_mid->SetTagTemp(mid_temp, true);
            m_chamber_target_temp = mid_temp;
        } else if (mid_temp < 0) {
            mid_temp = 0;
            m_tempCtrl_mid->SetTagTemp(mid_temp, true);
            m_chamber_target_temp = mid_temp;
        }

        Slic3r::GUI::ComTempCtrl* tempCtrl = new Slic3r::GUI::ComTempCtrl(mid_temp, top_temp, bottom_temp, 0);
        Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, tempCtrl);

        break;
    }
    }

}

wxBoxSizer* SingleDeviceState::create_monitoring_page()
{
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    //水平布局
    wxBoxSizer *bSizer_title_label = new wxBoxSizer(wxHORIZONTAL);
    auto panel_top_title = new wxPanel(this, wxID_ANY,wxDefaultPosition, wxSize(-1, FromDIP(22)), wxTAB_TRAVERSAL);
    panel_top_title->SetBackgroundColour(wxColour(240, 240, 240));
    //显示设备名称
    m_staticText_device_name = new Label(panel_top_title, (""));
    m_staticText_device_name->SetMinSize(wxSize(FromDIP(250),-1));
    m_staticText_device_name->SetMaxSize(wxSize(FromDIP(250), -1));
    m_staticText_device_name->SetForegroundColour(wxColour(51,51,51));

    bSizer_title_label->Add(m_staticText_device_name, 0, wxALIGN_LEFT| wxALL, 0);
    bSizer_title_label->AddStretchSpacer();

    //显示设备所在货架
    m_staticText_device_position = new Label(panel_top_title, (""));
    m_staticText_device_position->SetMinSize(wxSize(FromDIP(170), -1));
    m_staticText_device_position->SetMaxSize(wxSize(FromDIP(170), -1));
    m_staticText_device_position->SetForegroundColour(wxColour(51,51,51));

    bSizer_title_label->Add(m_staticText_device_position, 0, wxALIGN_LEFT | wxALL, 0);
    bSizer_title_label->AddStretchSpacer();

    //显示提示内容
    m_staticText_device_tip = new Label(panel_top_title, _L("error"));
    m_staticText_device_tip->SetForegroundColour(wxColour(251,71,71));

    bSizer_title_label->Add(m_staticText_device_tip, 0, wxALIGN_RIGHT | wxEXPAND | wxALL, 0);
    //bSizer_title_label->AddStretchSpacer();

    panel_top_title->SetSizer(bSizer_title_label);
    panel_top_title->Layout();
    bSizer_title_label->Fit(panel_top_title);

    sizer->AddSpacer(FromDIP(12));
    sizer->Add(panel_top_title, 0, wxEXPAND | wxALL, 0);
    sizer->AddSpacer(FromDIP(4));
    m_idleWnd.push_back(panel_top_title);
        
    //添加白色分割条
    auto m_panel_separotor_top = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(3)), wxTAB_TRAVERSAL);
    m_panel_separotor_top->SetBackgroundColour(wxColour(255,255,255));
    sizer->Add(m_panel_separotor_top, 0, wxEXPAND | wxALL, 0);
    m_idleWnd.push_back(m_panel_separotor_top);

    //第二段水平布局，相机垂直布局的顶部间隔
    auto m_panel_separotor0 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor0->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separotor0->SetMinSize(wxSize(-1, FromDIP(10)));
    m_panel_separotor0->SetMaxSize(wxSize(-1, FromDIP(10)));
    sizer->Add(m_panel_separotor0, 0, wxEXPAND, 0);
    //sizer->AddSpacer(FromDIP(6));
    m_idleWnd.push_back(m_panel_separotor0);

    //摄像头布局
    m_panel_monitoring_title = new wxPanel(this, wxID_ANY,wxDefaultPosition, wxSize(-1, FromDIP(36)), wxTAB_TRAVERSAL);
    m_panel_monitoring_title->SetBackgroundColour(wxColour(248,248,248));

    //“摄像头”文字布局
    wxBoxSizer *bSizer_monitoring_title;
    bSizer_monitoring_title = new wxBoxSizer(wxHORIZONTAL);

    m_staticText_monitoring = new Label(m_panel_monitoring_title, _L("Camera"));
    m_staticText_monitoring->SetForegroundColour(wxColour(51,51,51));

    bSizer_monitoring_title->AddSpacer(FromDIP(13));
    bSizer_monitoring_title->Add(m_staticText_monitoring, 0, wxTOP, FromDIP(6));
    bSizer_monitoring_title->AddStretchSpacer();

    m_panel_monitoring_title->SetSizer(bSizer_monitoring_title);
    m_panel_monitoring_title->Layout();
    bSizer_monitoring_title->Fit(m_panel_monitoring_title);

    //行与行之间的间距使用 wxPanel 进行填充
    sizer->Add(m_panel_monitoring_title, 0, wxEXPAND | wxALL, 0);
    m_idleWnd.push_back(m_panel_monitoring_title);

    //播放控件
    m_camera_play_url = wxString::Format("file://%s/web/orca/missing_connection.html?lang=http://192.168.4.64:8080/?action=stream", from_u8(resources_dir()));

    m_browser = WebView::CreateWebView(this,m_camera_play_url);

    wxEvtHandler::Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED, &SingleDeviceState::onScriptMessage, this);
    m_browser->Bind(wxEVT_WEBVIEW_NAVIGATED, &SingleDeviceState::on_navigated, this);
    if(m_browser == nullptr){
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format("load web view of SingleDeviceState url failed");
        return sizer;
    }
    m_browser->SetMinSize(wxSize(FromDIP(730), FromDIP(364)));
    //m_browser->SetMaxSize(wxSize(FromDIP(786), FromDIP(364)));
    m_browser->EnableContextMenu(true);
    //sizer->Add(m_browser, 1, wxEXPAND | wxALL, 0);
    sizer->Add(m_browser, 0, wxALL, 0);

    return sizer;
}

wxBoxSizer* SingleDeviceState::create_machine_control_title()
{
    wxBoxSizer *bSizer_v_title = new wxBoxSizer(wxVERTICAL);
    bSizer_v_title->AddSpacer(FromDIP(12));

    //设备提示信息
    wxBoxSizer *bSizer_h_title = new wxBoxSizer(wxHORIZONTAL);
    auto panel_top_right_info = new wxPanel(this, wxID_ANY,wxDefaultPosition, wxSize(-1, FromDIP(30)), wxTAB_TRAVERSAL);
    //panel_top_right_info = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    panel_top_right_info->SetBackgroundColour(wxColour(240, 240, 240));

    //显示报错信息
    //m_staticText_device_info = new Label(panel_top_right_info, ("error Info"), wxALIGN_CENTER);
    //m_staticText_device_info->Wrap(-1);
    ////m_staticText_device_info->SetFont(wxFont(wxFontInfo(16)));
    //m_staticText_device_info->SetBackgroundColour(wxColour(246,203,198));
    //m_staticText_device_info->SetForegroundColour(wxColour(251, 71, 71));

    //bSizer_h_title->Add(m_staticText_device_info, wxSizerFlags(1).Expand());
    //bSizer_h_title->AddSpacer(FromDIP(6));

    m_staticText_device_info = new FFScrollButton(panel_top_right_info, wxID_ANY, ("error Info"), 0);
    m_staticText_device_info->Enable(false);
    //m_staticText_device_info->SetBackgroundColour(*wxWHITE);
    m_staticText_device_info->SetBGDisableColor(wxColour("#F6CBC6"));
    m_staticText_device_info->SetFontDisableColor(wxColour("#FB4747"));
    m_staticText_device_info->SetMinSize(wxSize(FromDIP(394), FromDIP(56)));

    bSizer_h_title->Add(m_staticText_device_info, 0, wxALL | wxEXPAND);
    bSizer_h_title->AddSpacer(FromDIP(6));

    //显示清除按钮
    m_clear_button = new Button(panel_top_right_info, _L("clear"), "", 0, FromDIP(20));
    m_clear_button->SetPureText(true);
    m_clear_button->SetBorderWidth(1);
    m_clear_button->SetBackgroundColor(wxColour(240,240,240));
    m_clear_button->SetBorderColor(wxColour(50,141,251));
    m_clear_button->SetTextColor(wxColour(50,141,251));
    //m_clear_button->SetMinSize((wxSize(FromDIP(34), FromDIP(23))));
    m_clear_button->SetCornerRadius(0);
    m_clear_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) { 
        e.Skip();
        ComStateCtrl* stateCtrl = new ComStateCtrl(CLEAR_PLATFORM);
        Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, stateCtrl);
        Layout();
    });

    bSizer_h_title->Add(m_clear_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(0));

    panel_top_right_info->SetSizer(bSizer_h_title);
    panel_top_right_info->Layout();
    bSizer_h_title->Fit(panel_top_right_info);

    bSizer_v_title->Add(panel_top_right_info, 0, wxALL | wxEXPAND, 0);
    m_idleWnd.push_back(panel_top_right_info);

    //设备信息与信息之间的间隔
    auto m_panel_separotor3 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor3->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separotor3->SetMinSize(wxSize(-1, FromDIP(10)));
    m_panel_separotor3->SetMaxSize(wxSize(-1, FromDIP(10)));

    bSizer_v_title->Add(m_panel_separotor3, 0, wxEXPAND, 0);
    m_idleWnd.push_back(m_panel_separotor3);
    return bSizer_v_title;
}

wxBoxSizer* SingleDeviceState::create_machine_control_page()
{
    wxBoxSizer *bSizer_right = new wxBoxSizer(wxVERTICAL);
    bSizer_right->SetMinSize(wxSize(FromDIP(450),-1));

    //忙碌状态
    m_machine_ctrl_panel = new wxPanel(this);
    m_machine_ctrl_panel->SetBackgroundColour(*wxWHITE);
    m_machine_ctrl_panel->SetDoubleBuffered(true);
    wxBoxSizer* bSizer_v_busy = new wxBoxSizer(wxVERTICAL);
    setupLayoutBusyPage(bSizer_v_busy,m_machine_ctrl_panel);

    m_machine_ctrl_panel->SetSizer(bSizer_v_busy);
    m_machine_ctrl_panel->Layout();
    bSizer_v_busy->Fit(m_machine_ctrl_panel);
        
    //空闲状态
    m_machine_idle_panel = new wxPanel(this);
    //m_machine_idle_panel->SetBackgroundColour(*wxWHITE);
    m_machine_idle_panel->SetDoubleBuffered(true);
    wxBoxSizer* bSizer_v_idle = new wxBoxSizer(wxVERTICAL);
    setupLayoutIdlePage(bSizer_v_idle,m_machine_idle_panel);

    m_machine_idle_panel->SetSizer(bSizer_v_idle);
    m_machine_idle_panel->Layout();
    bSizer_v_idle->Fit(m_machine_idle_panel);
//***
    bSizer_right->Add(m_machine_ctrl_panel,  0, wxEXPAND, 0);
    bSizer_right->Add(m_machine_idle_panel, 0, wxEXPAND, 0);
    //m_machine_idle_panel->Hide();
    m_machine_ctrl_panel->Hide();
    m_idleWnd.push_back(m_machine_idle_panel);
    return bSizer_right;
}

void SingleDeviceState::setupLayout()
{
 //最外层框架布局
    wxBoxSizer *bSizer_status = new wxBoxSizer(wxVERTICAL);

    //顶部背景条（作为间距）
    auto m_panel_separotor_top_back = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(18)), wxTAB_TRAVERSAL);
    m_panel_separotor_top_back->SetBackgroundColour(wxColour(240,240,240));
    bSizer_status->Add(m_panel_separotor_top_back, 0, wxEXPAND | wxALL, 0);
    m_idleWnd.push_back(m_panel_separotor_top_back);

    //第二段水平布局
    wxBoxSizer *bSizer_status_below = new wxBoxSizer(wxHORIZONTAL);
    //第二段水平布局，左侧空白
    auto m_panel_separotor_left = new wxPanel(this, wxID_ANY, wxDefaultPosition,wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor_left->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separotor_left->SetMinSize(wxSize(FromDIP(24), -1));

    bSizer_status_below->Add(m_panel_separotor_left, 0, wxEXPAND | wxALL, 0);
    m_idleWnd.push_back(m_panel_separotor_left);
        
    //第二段水平布局，中间垂直布局
    wxBoxSizer *bSizer_left = new wxBoxSizer(wxVERTICAL);

    //第二段水平布局，相机布局
    auto m_monitoring_sizer = create_monitoring_page();
    bSizer_left->Add(m_monitoring_sizer, 0, wxALL, 0);

    //第二段水平布局，相机垂直布局的中间间隔
    auto m_panel_separotor1 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor1->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separotor1->SetMinSize(wxSize(-1, FromDIP(20)));
    m_panel_separotor1->SetMaxSize(wxSize(-1, FromDIP(20)));
    bSizer_left->Add(m_panel_separotor1, 0, wxALL, 0);
    m_idleWnd.push_back(m_panel_separotor1);

    //第二段水平布局，相机垂直布局中的材料站
    //MaterialStation高度指定为FromDIP(274)对应实际像素411，为与ui保持相同的宽高比
    m_material_station = new MaterialStation(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(274)));
    bSizer_left->Add(m_material_station, 0, wxALL | wxEXPAND, 0);

    bSizer_status_below->Add(bSizer_left, 0, wxALL | wxEXPAND, 0);
    m_idleWnd.push_back(m_material_station);
    m_idleWnd.push_back(m_material_station->GetPrintTitlePanel());

    //第二段水平布局，中间间隔
    auto m_panel_separator_middle = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxTAB_TRAVERSAL);
    m_panel_separator_middle->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separator_middle->SetMinSize(wxSize(FromDIP(20), -1));

    bSizer_status_below->Add(m_panel_separator_middle, 0, wxEXPAND | wxALL, 0);
    m_idleWnd.push_back(m_panel_separator_middle);

    //第二段水平布局，右侧信息
    //标题栏
    auto m_machine_title = create_machine_control_title();
    //信息与控制详情页

    auto m_machine_control = create_machine_control_page();
/*
    m_machine_ctrl_panel->SetSizer(m_machine_control);
    m_machine_ctrl_panel->Layout();
    m_machine_control->Fit(m_machine_ctrl_panel);

    bSizer_status_below->Add(m_machine_ctrl_panel, 0, wxALL, 0);
*/
    m_machine_title->Add(m_machine_control, 0, wxALL, 0);
    bSizer_status_below->Add(m_machine_title, 0, wxALL, 0);
    //水平布局最右侧间隔
    auto panel_separator_right = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(35), -1), wxTAB_TRAVERSAL);
    panel_separator_right->SetBackgroundColour(wxColour(240, 240, 240));

    bSizer_status_below->Add(panel_separator_right, 0, wxEXPAND | wxALL, 0);
    m_idleWnd.push_back(panel_separator_right);

    bSizer_status->Add(bSizer_status_below, 1, wxALL | wxEXPAND, 0);

    //底部间距
    auto panel_separotor_bottom = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(10)), wxTAB_TRAVERSAL);
    panel_separotor_bottom->SetBackgroundColour(wxColour(240, 240, 240));

    bSizer_status->Add(panel_separotor_bottom, 0, wxEXPAND | wxALL, 0);
    m_idleWnd.push_back(panel_separotor_bottom);
    this->SetSizerAndFit(bSizer_status);
    this->Layout();
}

void SingleDeviceState::setupLayoutBusyPage(wxBoxSizer* busySizer,wxPanel* parent)
{
    //标题：信息与控制
    auto panel_control_title = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(36)), wxTAB_TRAVERSAL);
    panel_control_title->SetBackgroundColour(wxColour(248, 248, 248));

    wxBoxSizer *bSizer_control_title = new wxBoxSizer(wxHORIZONTAL);
    auto staticText_control = new Label(panel_control_title, _L("Info and Control"));
    staticText_control->SetForegroundColour(wxColour(51, 51, 51));

    bSizer_control_title->Add(staticText_control, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(17));
    bSizer_control_title->AddStretchSpacer();

    panel_control_title->SetSizer(bSizer_control_title);
    panel_control_title->Layout();
    bSizer_control_title->Fit(panel_control_title);

    //添加标题
    busySizer->Add(panel_control_title, 0, wxALL | wxEXPAND, 0);
    ///********
    //先垂直布局（添加空白间距）
    //再水平布局（1、添加左侧空白间距；2、添加中间垂直布局，3、添加右侧垂直布局）
    //最终添加至整体布局

    wxBoxSizer *bSizer_file_info = new wxBoxSizer(wxVERTICAL);
    m_panel_control_info = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(167)), wxTAB_TRAVERSAL);
    m_panel_control_info->SetBackgroundColour(wxColour(255,255,255));

//***顶部白条（分隔）
    auto m_panel_separotor_top = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(8)), wxTAB_TRAVERSAL);
    m_panel_separotor_top->SetBackgroundColour(wxColour(255,255,255));

    bSizer_file_info->Add(m_panel_separotor_top, 0, wxEXPAND | wxALL, 0);

    //信息部分水平布局
    wxBoxSizer *bSizer_control_info = new wxBoxSizer(wxHORIZONTAL);

//***水平布局添加最左侧空白
    auto m_panel_separotor_left = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(17), -1),wxTAB_TRAVERSAL);
    m_panel_separotor_left->SetBackgroundColour(wxColour(255,255,255));
    //m_panel_separotor_left->SetMinSize(wxSize(FromDIP(10), -1));

    bSizer_control_info->Add(m_panel_separotor_left, 0, wxEXPAND | wxALL, 0);

//添加垂直布局（显示文件内容）
    wxBoxSizer *bSizer_control_file_info = new wxBoxSizer(wxVERTICAL);
    //auto m_panel_control_file_info = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(145)), wxTAB_TRAVERSAL);
    auto m_panel_control_file_info = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxDefaultSize,wxTAB_TRAVERSAL);
    m_panel_control_file_info->SetBackgroundColour(wxColour(255,255,255));

//文件信息
    wxBoxSizer *bSizer_control_file_name = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_control_file_name = new wxPanel(m_panel_control_file_info, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_control_file_name->SetBackgroundColour(wxColour(255, 255, 255));

    //显示文件标题
    m_staticText_file_head = new Label(m_panel_control_file_name, _L("file: "));
    m_staticText_file_head->SetForegroundColour(wxColour(51, 51, 51));

    //显示文件名称
    m_staticText_file_name = new wxStaticText(m_panel_control_file_name, wxID_ANY, "");
    m_staticText_file_name->SetForegroundColour(wxColour(51, 51, 51));

    bSizer_control_file_name->Add(m_staticText_file_head);
    bSizer_control_file_name->Add(m_staticText_file_name);
    m_panel_control_file_name->SetSizer(bSizer_control_file_name);
    m_panel_control_file_name->Layout();
    bSizer_control_file_name->Fit(m_panel_control_file_name);


    bSizer_control_file_info->Add(m_panel_control_file_name, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
/*       
    //显示设备状态
    m_staticText_device_state = new Label(m_panel_control_file_info, _L("pause"));
    m_staticText_device_state->SetForegroundColour(wxColour(50,141,251));

    bSizer_control_file_info->Add(m_staticText_device_state, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
*/

////文件中间白条（分隔）
//        auto m_panel_separotor_mid = new wxPanel(m_panel_control_file_info, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(24)), wxTAB_TRAVERSAL);
//        m_panel_separotor_mid->SetBackgroundColour(wxColour(255,255,255));
//
//        bSizer_control_file_info->Add(m_panel_separotor_mid, 0, wxEXPAND | wxALL, 0);
    //bSizer_control_file_info->AddStretchSpacer();
    bSizer_control_file_info->AddSpacer(FromDIP(40));

    //显示倒计时
    wxString time = "0" + _L("h ") + "0" + _L("min ");
    m_staticText_count_time = new Label(m_panel_control_file_info, ::Label::Body_14, time);
    m_staticText_count_time->Wrap(-1);
    //m_staticText_count_time->SetFont(wxFont(wxFontInfo(16)));
    m_staticText_count_time->SetForegroundColour(wxColour(50,141,251));

    bSizer_control_file_info->Add(m_staticText_count_time, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_file_info->AddSpacer(FromDIP(10));

    //显示剩余时间标签
    m_staticText_time_label = new Label(m_panel_control_file_info, ::Label::Body_12, _L("Remaining Time"));
    m_staticText_time_label->SetForegroundColour(wxColour(153,153,153));

    bSizer_control_file_info->Add(m_staticText_time_label, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));

    auto m_panel_separotor_mid = new wxPanel(m_panel_control_file_info, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(11)),wxTAB_TRAVERSAL);
    m_panel_separotor_mid->SetBackgroundColour(wxColour(255, 255, 255));

    bSizer_control_file_info->Add(m_panel_separotor_mid, 0, wxEXPAND | wxALL, 0);

    //显示进度条
    m_progress_bar = new ProgressBar(m_panel_control_file_info,wxID_ANY,100,wxDefaultPosition,wxSize(FromDIP(201),FromDIP(5)),true);
    m_progress_bar->SetProgress(30);
    m_progress_bar->SetProgressBackgroundColour(wxColour(50, 141, 251));
    bSizer_control_file_info->Add(m_progress_bar, 0, wxALIGN_CENTER_VERTICAL, FromDIP(4));
    //bSizer_control_file_info->AddSpacer(FromDIP(6));

    m_panel_control_file_info->SetSizer(bSizer_control_file_info);
    m_panel_control_file_info->Layout();
    bSizer_control_file_info->Fit(m_panel_control_file_info);
        
    bSizer_control_info->Add(m_panel_control_file_info, 0, wxEXPAND | wxALL, 0);
/*
//***水平布局添加中间空白间距
    auto m_panel_separotor_right = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(58), -1), wxTAB_TRAVERSAL);
    m_panel_separotor_right->SetBackgroundColour(wxColour(255,255,255));

    bSizer_control_info->Add(m_panel_separotor_right, 0, wxEXPAND | wxALL, 0);
*/
    bSizer_control_info->AddSpacer(FromDIP(125));

//***添加右侧垂直布局
    wxBoxSizer *bSizer_control_material = new wxBoxSizer(wxVERTICAL);
    m_panel_control_material = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(145)), wxTAB_TRAVERSAL);
    m_panel_control_material->SetBackgroundColour(wxColour(255,255,255));

    //***添加右侧材料
    static Slic3r::GUI::BitmapCache cache;
    auto material_weight_pic = create_scaled_bitmap("device_material_weight", this, 16);
        
    m_material_weight_staticbitmap = new wxStaticBitmap(m_panel_control_material, wxID_ANY, material_weight_pic);

    m_material_picture = new MaterialImagePanel(m_panel_control_material);
    m_material_picture->SetMinSize(wxSize(FromDIP(80), FromDIP(80)));

    m_material_weight_label = new Label(m_panel_control_material,("234g"));
        
    wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);
    hbox->SetMinSize(wxSize(60,-1));
    hbox->Add(m_material_weight_staticbitmap,0, wxALIGN_CENTER| wxALL,0);
    hbox->AddSpacer(FromDIP(5));
    hbox->Add(m_material_weight_label, wxALIGN_CENTER| wxALL,0);

    bSizer_control_material->Add(hbox, 0, /*wxALIGN_CENTER*/ wxRIGHT | wxALL, 0);
    bSizer_control_material->AddStretchSpacer();
    bSizer_control_material->Add(m_material_picture, 0, /*wxALIGN_CENTER*/ wxRIGHT, 0);

    m_panel_control_material->SetSizer(bSizer_control_material);
    m_panel_control_material->Layout();
    bSizer_control_material->Fit(m_panel_control_material);
        
    bSizer_control_info->Add(m_panel_control_material);

//***水平布局添加最右侧空白
    auto m_panel_separotor_right2 = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition,wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor_right2->SetBackgroundColour(wxColour(255,255,255));
    m_panel_separotor_right2->SetMinSize(wxSize(FromDIP(26), -1));

    bSizer_control_info->Add(m_panel_separotor_right2, 0, wxEXPAND | wxALL, 0);

    bSizer_file_info->Add(bSizer_control_info, 0, wxEXPAND | wxALL, 0);

//**信息与控制布局添加至垂直布局
    m_panel_control_info->SetSizer(bSizer_file_info);
    m_panel_control_info->Layout();
    bSizer_file_info->Fit(m_panel_control_info);

    busySizer->Add(m_panel_control_info, 0, wxALL | wxEXPAND, 0);

//****添加暂停打印、取消打印
    //设备信息与打印按钮之间的间隔
    auto m_panel_separotor4 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor4->SetBackgroundColour(wxColour(248,248,248));
    m_panel_separotor4->SetMinSize(wxSize(-1, FromDIP(13)));
    m_panel_separotor4->SetMaxSize(wxSize(-1, FromDIP(13)));
    busySizer->Add(m_panel_separotor4, 0, wxEXPAND, 0);

//***打印布局
    wxBoxSizer *bSizer_control_print = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_control_print = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(52)), wxTAB_TRAVERSAL);
    m_panel_control_print->SetBackgroundColour(wxColour(255,255,255));

    //显示继续打印按钮
    m_print_button = new Button(m_panel_control_print, _L("pause print"), "device_pause_print", 0, 16);
    m_print_button->SetFlashForge(true);
    m_print_button->SetBorderWidth(0);
    m_print_button->SetBackgroundColor(wxColour(255,255,255));
    m_print_button->SetBorderColor(wxColour(255,255,255));
    m_print_button->SetTextColor(wxColour(51,51,51));
//  m_print_button->SetMinSize((wxSize(FromDIP(158), FromDIP(29))));
    m_print_button->SetCornerRadius(0);
    m_print_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        e.Skip();
        auto detail = Slic3r::GUI::MultiComMgr::inst()->devData(m_cur_id).devDetail;
        std::string status = detail->status;
        if (/*!m_print_button_pressed_down*/ status == "printing") {
            m_cur_printing_ctrl    = 1;
            std::string printState = PAUSE;
            std::string jobId      = Slic3r::GUI::MultiComMgr::inst()->devData(m_cur_id).devDetail->jobId;
            ComJobCtrl *jobCtrl    = new ComJobCtrl(jobId, printState);
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, jobCtrl);
            /*m_print_button->SetLabel(_L("continue print"));
            m_print_button->SetIcon("device_continue_print");
            m_print_button->Refresh();*/
        } else if (status == PAUSE || status == P_PAUSING) {
            m_cur_printing_ctrl    = 2;
            std::string printState = CONTINUE;
            std::string jobId      = Slic3r::GUI::MultiComMgr::inst()->devData(m_cur_id).devDetail->jobId;
            ComJobCtrl *jobCtrl = new ComJobCtrl(jobId, printState);
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, jobCtrl);
            /*m_print_button->SetLabel(_L("pause print"));
            m_print_button->SetIcon("device_pause_print");
            m_print_button->Refresh();*/
        }
        //m_print_button_pressed_down = !m_print_button_pressed_down;
    });

    //bSizer_control_print->Add(m_print_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_print->Add(m_print_button, wxSizerFlags(1).Expand());

    auto m_panel_separotor_print = new wxPanel(m_panel_control_print, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(6), -1), wxTAB_TRAVERSAL);
    m_panel_separotor_print->SetBackgroundColour(wxColour(240,240,240));

    bSizer_control_print->Add(m_panel_separotor_print, 0, wxEXPAND | wxALL, 0);

    //显示取消打印按钮
    m_cancel_button = new Button(m_panel_control_print, _L("cancel print"), "device_cancel_print", 0, 16);
    m_cancel_button->SetFlashForge(true);
    m_cancel_button->SetBorderWidth(0);
    m_cancel_button->SetBackgroundColor(wxColour(255,255,255));
    m_cancel_button->SetBorderColor(wxColour(255,255,255));
    m_cancel_button->SetTextColor(wxColour(51,51,51));
//  m_cancel_button->SetMinSize((wxSize(FromDIP(158), FromDIP(29))));
    m_cancel_button->SetCornerRadius(0);
    m_cancel_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) { 
        //e.Skip();
        if (!m_cancel_confirm_page) {
            m_cancel_confirm_page = new CancelPrint(_L("Whether Cancel Printing"), _L("yes"), _L("no"));
            m_cancel_confirm_page->Bind(EVT_CANCEL_PRINT_CLICKED, &SingleDeviceState::onCancelPrint,this);
            m_cancel_confirm_page->Bind(EVT_CONTINUE_PRINT_CLICKED, &SingleDeviceState::onContinuePrint,this);
        }
        m_cancel_confirm_page->ShowModal();
    });

    //bSizer_control_print->Add(m_cancel_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_print->Add(m_cancel_button, wxSizerFlags(1).Expand());

//***继续打印布局添加至垂直布局
    m_panel_control_print->SetSizer(bSizer_control_print);
    m_panel_control_print->Layout();
    bSizer_control_print->Fit(m_panel_control_print);

    busySizer->Add(m_panel_control_print,0, wxALL | wxEXPAND, 0);

//***添加打印控制和温度布局之间的间隔
    auto m_panel_separotor5 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor5->SetBackgroundColour(wxColour(240,240,240));
    m_panel_separotor5->SetMinSize(wxSize(-1, FromDIP(13)));
    m_panel_separotor5->SetMaxSize(wxSize(-1, FromDIP(13)));
    busySizer->Add(m_panel_separotor5, 0, wxEXPAND, 0);

//***温度布局
    wxBoxSizer *bSizer_control_temperature = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_control_temperature = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(52)), wxTAB_TRAVERSAL);
    m_panel_control_temperature->SetBackgroundColour(wxColour(255,255,255));

    //显示顶部温度控件
    wxWindowID top_id = wxWindow::NewControlId();
    m_tempCtrl_top = new TempInput(m_panel_control_temperature, top_id, wxString("--"), wxString("--"), wxString("device_top_temperature"), wxString("device_top_temperature"), wxDefaultPosition,
                                    wxDefaultSize, wxALIGN_CENTER);
    m_tempCtrl_top->SetMinTemp(20);
    m_tempCtrl_top->SetMaxTemp(120);
    m_tempCtrl_top->SetMinSize((wxSize(FromDIP(106), FromDIP(29))));
    m_tempCtrl_top->SetBorderWidth(0);
    StateColor tempinput_text_colour(std::make_pair(wxColour(51,51,51), (int) StateColor::Disabled), std::make_pair(wxColour(48,58,60), (int) StateColor::Normal));
    m_tempCtrl_top->SetTextColor(tempinput_text_colour);
    StateColor tempinput_border_colour(std::make_pair(*wxWHITE, (int)StateColor::Disabled), std::make_pair(wxColour(0, 150, 136), (int)StateColor::Focused),
                                    std::make_pair(wxColour(0, 150, 136), (int)StateColor::Hovered),std::make_pair(*wxWHITE, (int)StateColor::Normal));
    m_tempCtrl_top->SetBorderColor(tempinput_border_colour);
    m_tempCtrl_top->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &event) {
        event.Skip();
        lostFocusmodifyTemp();
    });
    m_tempCtrl_top->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &event) {
        event.Skip();
        lostFocusmodifyTemp();
    });
    //m_tempCtrl_top->Bind(wxEVT_TEXT,&SingleDeviceState::onTargetTempModify, this);  

    //bSizer_control_temperature->Add(m_tempCtrl_top, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_temperature->Add(m_tempCtrl_top, wxSizerFlags(1).Expand());

    //间距
    auto m_panel_separotor_temp = new wxPanel(m_panel_control_temperature, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(6), -1), wxTAB_TRAVERSAL);
    m_panel_separotor_temp->SetBackgroundColour(wxColour(240,240,240));
    bSizer_control_temperature->Add(m_panel_separotor_temp, 0, wxEXPAND | wxALL, 0);

    //显示底部温度控件
    wxWindowID bottom_id = wxWindow::NewControlId();
    m_tempCtrl_bottom = new TempInput(m_panel_control_temperature, bottom_id, wxString("--"), wxString("--"), wxString("device_bottom_temperature"), wxString("device_bottom_temperature"), wxDefaultPosition,
                                    wxDefaultSize, wxALIGN_CENTER);

    m_tempCtrl_bottom->SetMinTemp(20);
    m_tempCtrl_bottom->SetMaxTemp(120);
    m_tempCtrl_bottom->SetMinSize((wxSize(FromDIP(106), FromDIP(29))));
    m_tempCtrl_bottom->SetBorderWidth(0);
    //StateColor tempinput_text_colour(std::make_pair(wxColour(171, 172, 172), (int) StateColor::Disabled), std::make_pair(wxColour(48,58,60), (int) StateColor::Normal));
    //m_tempCtrl_bottom->SetTextColor(tempinput_text_colour);
    //StateColor tempinput_border_colour(std::make_pair(*wxWHITE, (int)StateColor::Disabled), std::make_pair(wxColour(0, 150, 136), (int)StateColor::Focused),
    //                                std::make_pair(wxColour(0, 150, 136), (int)StateColor::Hovered),std::make_pair(*wxWHITE, (int)StateColor::Normal));
    m_tempCtrl_bottom->SetBorderColor(tempinput_border_colour);
    m_tempCtrl_bottom->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &event) {
        event.Skip();
        lostFocusmodifyTemp();
    });
    m_tempCtrl_bottom->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &event) {
        event.Skip();
        lostFocusmodifyTemp();
    });
    //m_tempCtrl_bottom->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);  
    //bSizer_control_temperature->Add(m_tempCtrl_bottom, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_temperature->Add(m_tempCtrl_bottom, wxSizerFlags(1).Expand());

    //间距
    auto m_panel_separotor_temp1 = new wxPanel(m_panel_control_temperature, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(6), -1), wxTAB_TRAVERSAL);
    m_panel_separotor_temp1->SetBackgroundColour(wxColour(240,240,240));
    bSizer_control_temperature->Add(m_panel_separotor_temp1, 0, wxEXPAND | wxALL, 0);

    //显示中间温度控件
    wxWindowID bottom_mid = wxWindow::NewControlId();
    m_tempCtrl_mid = new TempInput(m_panel_control_temperature, bottom_mid, wxString("--"), wxString("--"), wxString("device_mid_temperature"), wxString("device_mid_temperature"), wxDefaultPosition,
                                    wxDefaultSize, wxALIGN_CENTER);

    m_tempCtrl_mid->SetMinTemp(20);
    m_tempCtrl_mid->SetMaxTemp(120);
    m_tempCtrl_mid->SetMinSize((wxSize(FromDIP(106), FromDIP(29))));
    m_tempCtrl_mid->SetBorderWidth(0);
    m_tempCtrl_mid->SetReadOnly(true);
    //m_tempCtrl_mid->SetTextBindInput();
    //StateColor tempinput_text_colour(std::make_pair(wxColour(171, 172, 172), (int) StateColor::Disabled), std::make_pair(wxColour(48,58,60), (int) StateColor::Normal));
    //m_tempCtrl_mid->SetTextColor(tempinput_text_colour);
    //StateColor tempinput_border_colour(std::make_pair(*wxWHITE, (int)StateColor::Disabled), std::make_pair(wxColour(0, 150, 136), (int)StateColor::Focused),
    //                                std::make_pair(wxColour(0, 150, 136), (int)StateColor::Hovered),std::make_pair(*wxWHITE, (int)StateColor::Normal));
    m_tempCtrl_mid->SetBorderColor(tempinput_border_colour);
    //m_tempCtrl_mid->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
    m_tempCtrl_mid->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &event) {
        event.Skip();
        lostFocusmodifyTemp();
    });
    m_tempCtrl_mid->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &event) {
        event.Skip();
        lostFocusmodifyTemp();
    });
    //bSizer_control_temperature->Add(m_tempCtrl_mid, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_temperature->Add(m_tempCtrl_mid, wxSizerFlags(1).Expand());

//***温度布局添加至垂直布局
    m_panel_control_temperature->SetSizer(bSizer_control_temperature);
    m_panel_control_temperature->Layout();
    bSizer_control_temperature->Fit(m_panel_control_temperature);

    busySizer->Add(m_panel_control_temperature,0, wxALL | wxEXPAND, 0);

//***添加温度布局和灯布局之间的间隔
    auto m_panel_separotor6 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor6->SetBackgroundColour(wxColour(240,240,240));
    m_panel_separotor6->SetMinSize(wxSize(-1, FromDIP(13)));
    m_panel_separotor6->SetMaxSize(wxSize(-1, FromDIP(13)));
    busySizer->Add(m_panel_separotor6, 0, wxEXPAND, 0);

//***灯控制布局
    wxBoxSizer *bSizer_control_lamp = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_control_lamp = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(52)), wxTAB_TRAVERSAL);
    m_panel_control_lamp->SetBackgroundColour(wxColour(255,255,255));

    //显示文件信息按钮
    m_device_info_button = new Button(m_panel_control_lamp, wxString(""), "device_file_info", 0, FromDIP(18));
    m_device_info_button->SetBorderWidth(0);
    m_device_info_button->SetBackgroundColor(wxColour(255,255,255));
    m_device_info_button->SetBorderColor(wxColour(255,255,255));
    //m_device_info_button->SetTextColor(wxColour(51,51,51));
    m_device_info_button->SetMinSize((wxSize(FromDIP(108), FromDIP(29))));
    m_device_info_button->SetCornerRadius(0);
    //bSizer_control_lamp->Add(m_device_info_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_lamp->Add(m_device_info_button, wxSizerFlags(1).Expand());
    bSizer_control_lamp->AddSpacer(FromDIP(35));

    //显示灯控制按钮
    m_lamp_control_button = new Button(m_panel_control_lamp, wxString(""), "device_lamp_control", 0, FromDIP(18));
    m_lamp_control_button->SetBorderWidth(0);
    m_lamp_control_button->SetBackgroundColor(wxColour(255,255,255));
    m_lamp_control_button->SetBorderColor(wxColour(255,255,255));
    //m_lamp_control_button->SetTextColor(wxColour(51,51,51));
    m_lamp_control_button->SetMinSize((wxSize(FromDIP(108), FromDIP(29))));
    m_lamp_control_button->SetCornerRadius(0);
    //bSizer_control_lamp->Add(m_lamp_control_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_lamp->Add(m_lamp_control_button, wxSizerFlags(1).Expand());
    bSizer_control_lamp->AddSpacer(FromDIP(35));

    //显示过滤按钮
    m_filter_button = new Button(m_panel_control_lamp, wxString(""), "device_filter", 0, FromDIP(18));
    m_filter_button->SetBorderWidth(0);
    m_filter_button->SetBackgroundColor(wxColour(255,255,255));
    m_filter_button->SetBorderColor(wxColour(255,255,255));
    //m_filter_button->SetTextColor(wxColour(51,51,51));
    m_filter_button->SetMinSize((wxSize(FromDIP(108), FromDIP(29))));
    m_filter_button->SetCornerRadius(0);
    //bSizer_control_lamp->Add(m_filter_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_lamp->Add(m_filter_button, wxSizerFlags(1).Expand());

//***灯控制布局添加至垂直布局
    m_panel_control_lamp->SetSizer(bSizer_control_lamp);
    m_panel_control_lamp->Layout();
    bSizer_control_lamp->Fit(m_panel_control_lamp);

    busySizer->Add(m_panel_control_lamp,0, wxALL | wxEXPAND, 0);

//**灯控制布局和设备详情之间间隔
    auto m_panel_separotor7 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor7->SetBackgroundColour(wxColour(240,240,240));
    m_panel_separotor7->SetMinSize(wxSize(-1, FromDIP(21)));
    m_panel_separotor7->SetMaxSize(wxSize(-1, FromDIP(21)));
    busySizer->Add(m_panel_separotor7, 0, wxEXPAND, 0);

//添加设备详情
    m_busy_device_detial = new DeviceDetail(parent);
    m_busy_device_detial->Bind(EVT_SWITCH_TO_FILETER, [this](wxCommandEvent &event) {
        event.Skip();
        m_busy_device_detial->switchPage();
    });
    busySizer->Add(m_busy_device_detial, 0, wxALL, 0);
    m_busy_device_detial->Hide();

    m_busy_G3U_detail = new G3UDetail(parent);
    m_busy_G3U_detail->Bind(EVT_SWITCH_TO_FILETER, [this](wxCommandEvent& event) {
        event.Skip();
        m_busy_G3U_detail->switchPage();
    });
    busySizer->Add(m_busy_G3U_detail, 0, wxALL, 0);
    m_busy_G3U_detail->Hide();

//添加循环过滤
    m_busy_circula_filter = new StartFilter(parent);
    busySizer->Add(m_busy_circula_filter, 0, wxALL, 0);
    m_busy_circula_filter->Hide();
//添加温度修改确认页面
    m_busy_temp_brn = new ModifyTemp(parent);
    m_busy_temp_brn->Bind(EVT_MODIFY_TEMP_CLICKED, &SingleDeviceState::onModifyTempClicked,this);
    m_busy_temp_brn->Bind(EVT_MODIFY_TEMP_CANCEL_CLICKED, [this](wxCommandEvent &event) {
        event.Skip();
        m_busy_temp_brn->Hide();
        m_tempCtrl_top->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
        m_tempCtrl_top->SetTagTemp(m_right_target_temp, true);
        m_tempCtrl_top->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this); 
        m_tempCtrl_bottom->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
        m_tempCtrl_bottom->SetTagTemp(m_plat_target_temp, true);
        m_tempCtrl_bottom->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
        Layout();
    });
    busySizer->Add(m_busy_temp_brn, 0, wxALL | wxEXPAND, 0);
    m_busy_temp_brn->Hide();
}

void SingleDeviceState::setupLayoutIdlePage(wxBoxSizer* idleSizer,wxPanel* parent)
{
    //标题：信息与控制
    auto panel_control_title2 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(36)), wxTAB_TRAVERSAL);
    panel_control_title2->SetBackgroundColour(wxColour(248, 248, 248));

    wxBoxSizer *bSizer_control_title = new wxBoxSizer(wxHORIZONTAL);
    auto staticText_control2 = new Label(panel_control_title2, _L("Info and Control"));
    staticText_control2->Wrap(-1);
    staticText_control2->SetForegroundColour(wxColour(51, 51, 51));
    m_idleWnd.push_back(staticText_control2);

    bSizer_control_title->Add(staticText_control2, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(17));
    bSizer_control_title->AddStretchSpacer();

    panel_control_title2->SetSizer(bSizer_control_title);
    panel_control_title2->Layout();
    bSizer_control_title->Fit(panel_control_title2);

    //添加标题
    idleSizer->Add(panel_control_title2, 0, wxALL | wxEXPAND, 0);
    m_idleWnd.push_back(panel_control_title2);

    m_busyState_top_gap = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_busyState_top_gap->SetBackgroundColour(wxColour(255,255,255));
    m_busyState_top_gap->SetMinSize(wxSize(-1, FromDIP(28)));
    idleSizer->Add(m_busyState_top_gap, 0, wxALL | wxEXPAND, 0);
    m_busyState_top_gap->Hide();

    //水平布局，机器图 + 文字
    wxBoxSizer *bSizer_h_device_tip = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *bSizer_v_device_text = new wxBoxSizer(wxVERTICAL);
    m_panel_idle = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_idle->SetBackgroundColour(*wxWHITE);
    //m_panel_idle->SetMinSize(wxSize(-1,FromDIP(178)));
        
    auto idle_device_pic = create_scaled_bitmap("adventurer_5m", 0, 165);
    m_idle_device_staticbitmap = new wxStaticBitmap(m_panel_idle, wxID_ANY, idle_device_pic);
    m_staticText_idle = new Label(m_panel_idle, HAS_NO_PRINTING);
    //splitIdleTextLabel();
    m_staticText_idle->SetForegroundColour(wxColour(51,51,51));
    m_staticText_idle->SetBackgroundColour(wxColour(255,255,255));
    m_staticText_idle->SetWindowStyleFlag(wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL); 
    m_idleWnd.push_back(m_staticText_idle);
    m_idleWnd.push_back(m_idle_device_staticbitmap);

    bSizer_h_device_tip->Add(m_idle_device_staticbitmap,0, wxALL | wxEXPAND, 0);
    bSizer_v_device_text->Add(m_staticText_idle,0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
    bSizer_h_device_tip->Add(bSizer_v_device_text,0,wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL);
    bSizer_h_device_tip->AddStretchSpacer();

    m_panel_idle->SetSizer(bSizer_h_device_tip);
    m_panel_idle->Layout();
    bSizer_h_device_tip->Fit(m_panel_idle);

    idleSizer->Add(m_panel_idle, 0, wxALL | wxEXPAND, 0);
    m_idleWnd.push_back(m_panel_idle);

    //添加空白间距
    auto m_panel_separotor1 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor1->SetBackgroundColour(wxColour(255,255,255));
    m_panel_separotor1->SetMinSize(wxSize(-1, FromDIP(8)));

    m_busyState_bottom_gap = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_busyState_bottom_gap->SetBackgroundColour(wxColour(255, 255, 255));
    m_busyState_bottom_gap->SetMinSize(wxSize(-1, FromDIP(36)));
    idleSizer->Add(m_busyState_bottom_gap, 0, wxALL | wxEXPAND, 0);
    m_busyState_bottom_gap->Hide();

    idleSizer->Add(m_panel_separotor1, 0, wxALL | wxEXPAND, 0);
    m_idleWnd.push_back(m_panel_separotor1);

//*** 设备空闲和文件列表间距
    //添加空白间距
    auto m_panel_separotor2 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor2->SetBackgroundColour(wxColour(240,240,240));
    m_panel_separotor2->SetMinSize(wxSize(-1, FromDIP(10)));

    idleSizer->Add(m_panel_separotor2, 0, wxALL | wxEXPAND, 0);
    m_idleWnd.push_back(m_panel_separotor2);

    m_panel_idle_text = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(450), FromDIP(52)), wxTAB_TRAVERSAL);
    m_fileListbutton  = new Button(m_panel_idle_text, _L("Local File List"), "local_file_list", 0, 16);
    m_fileListbutton->SetMinSize((wxSize(FromDIP(225), FromDIP(45))));
    m_fileListbutton->SetFlashForge(true);
    m_fileListbutton->SetBorderWidth(0);
    m_fileListbutton->SetBackgroundColor(wxColour(255, 255, 255));
    m_fileListbutton->SetBorderColor(wxColour(255, 255, 255));
    m_fileListbutton->SetTextColor(wxColour(51, 51, 51));
    m_fileListbutton->SetCornerRadius(0);

    m_timeLapseVideoBtn = new Button(m_panel_idle_text, _L("Time-Lapse Video"), "time_lapse_video", 0, 16);
    m_timeLapseVideoBtn->SetMinSize((wxSize(FromDIP(225), FromDIP(45))));
    m_timeLapseVideoBtn->SetFlashForge(true);
    m_timeLapseVideoBtn->SetBorderWidth(0);
    m_timeLapseVideoBtn->SetBackgroundColor(wxColour(255, 255, 255));
    m_timeLapseVideoBtn->SetBorderColor(wxColour(255, 255, 255));
    m_timeLapseVideoBtn->SetTextColor(wxColour(51, 51, 51));
    m_timeLapseVideoBtn->SetCornerRadius(0);

    wxBoxSizer* bSizer_h_idle_text = new wxBoxSizer(wxHORIZONTAL);
    bSizer_h_idle_text->Add(m_fileListbutton, 0, wxEXPAND);
    bSizer_h_idle_text->AddSpacer(FromDIP(6));
    bSizer_h_idle_text->Add(m_timeLapseVideoBtn, 0, wxEXPAND);

    m_panel_idle_text->SetSizer(bSizer_h_idle_text);
    idleSizer->Add(m_panel_idle_text, 0, wxALL | wxEXPAND, 0);

    //*** 文件列表和列表内容间距
    // 添加空白间距
    m_panel_separotor8 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor8->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separotor8->SetMinSize(wxSize(-1, FromDIP(10)));

    idleSizer->Add(m_panel_separotor8, 0, wxALL | wxEXPAND, 0);
    m_idleWnd.push_back(m_panel_separotor8);
    //m_panel_separotor8->Hide();

    m_scrolledWindow = new wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL | wxVSCROLL);
    //m_scrolledWindow->SetBackgroundColour(/**wxWHITE*/ wxColour("#fafafa"));
    m_scrolledWindow->SetBackgroundColour(*wxWHITE);
    m_scrolledWindow->SetMinSize(wxSize(FromDIP(450), FromDIP(318)));
    m_scrolledWindow->SetScrollRate(0, 30);
    m_sizer_my_devices = new wxBoxSizer(wxVERTICAL);
    m_scrolledWindow->SetSizer(m_sizer_my_devices);
    m_scrolledWindow->Layout();
    m_sizer_my_devices->Fit(m_scrolledWindow);

    idleSizer->Add(m_scrolledWindow, 0, wxALL | wxEXPAND, 0);

    m_scrolledWindow->Hide();
//split line
    wxSize size = GetSize();
    m_FileList_split_line = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(size.x, FromDIP(1)), wxTAB_TRAVERSAL);
    m_FileList_split_line->SetBackgroundColour(wxColor("#F0F0F0"));
    idleSizer->Add(m_FileList_split_line, 0, wxALL | wxEXPAND, 0);
    m_FileList_split_line->Hide();

// print btn
    wxBoxSizer* bSizer_v_print = new wxBoxSizer(wxVERTICAL);
    m_panel_print_btn          = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(450), FromDIP(92)), wxTAB_TRAVERSAL);
    m_panel_print_btn->SetBackgroundColour(wxColor("#FFFFFF"));

// wxHORIZONTAL 
    wxBoxSizer* bSizer_h_print  = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_control_print = new wxPanel(m_panel_print_btn, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(40)), wxTAB_TRAVERSAL);
    m_panel_control_print->SetBackgroundColour(wxColour("#FFFFFF"));

    m_printBtn = new FFButton(m_panel_control_print, wxID_ANY, _L("print"));
    m_printBtn->SetMinSize(wxSize(FromDIP(64), FromDIP(32)));
    m_printBtn->SetFontHoverColor(wxColour(255, 255, 255));
    m_printBtn->SetBGHoverColor(wxColour(149, 197, 255));
    m_printBtn->SetBorderHoverColor(wxColour(149, 197, 255));

    m_printBtn->SetFontPressColor(wxColour(255, 255, 255));
    m_printBtn->SetBGPressColor(wxColour(17, 111, 223));
    m_printBtn->SetBorderPressColor(wxColour(17, 111, 223));

    m_printBtn->SetFontColor(wxColour(255, 255, 255));
    m_printBtn->SetBorderColor(wxColour(50, 141, 251));
    m_printBtn->SetBGColor(wxColour(50, 141, 251));
    m_printBtn->Enable(false);
    m_printBtn->Bind(wxEVT_LEFT_DOWN, &SingleDeviceState::onFileListPrintBtnClicked, this);
    
    bSizer_h_print->Add(m_printBtn, 0, wxALIGN_CENTER, 0);

    m_refreshBtn = new Button(m_panel_control_print, "", "file_list_refresh", 0, 25);
    //m_refreshBtn->SetMinSize((wxSize(FromDIP(450), FromDIP(45))));
    //m_refreshBtn->SetBorderWidth(0);
    m_refreshBtn->SetFlashForge(true);
    m_refreshBtn->SetBackgroundColor(wxColour(255, 255, 255));
    m_refreshBtn->SetBorderColor(wxColour(255, 255, 255));
    m_refreshBtn->Bind(wxEVT_LEFT_DOWN, &SingleDeviceState::onFileListRefreshBtnClicked, this);

    bSizer_h_print->AddSpacer(FromDIP(71));
    bSizer_h_print->Add(m_refreshBtn, 0, wxALIGN_CENTER, 0);


    m_panel_control_print->SetSizer(bSizer_h_print);
    m_panel_control_print->Layout();
    bSizer_h_print->Fit(m_panel_control_print);

    bSizer_v_print->AddStretchSpacer();
    bSizer_v_print->Add(m_panel_control_print, 0, wxALIGN_CENTER, 0);
    bSizer_v_print->AddStretchSpacer();

    m_panel_print_btn->SetSizer(bSizer_v_print);
    m_panel_print_btn->Layout();
    bSizer_v_print->Fit(m_panel_print_btn);

    idleSizer->Add(m_panel_print_btn, 0, wxCENTER, 0);
    m_panel_print_btn->Hide();

    //延迟视频
    m_timeLapseVideoPnl = new TimeLapseVideoPanel(parent);
    m_timeLapseVideoPnl->SetBackgroundColour(*wxWHITE);
    m_timeLapseVideoPnl->SetMinSize(wxSize(FromDIP(450), FromDIP(411)));
    m_timeLapseVideoPnl->Hide();
    idleSizer->Add(m_timeLapseVideoPnl, 0, wxALL | wxEXPAND, 0);

    //新增温度-设备控件
    m_idle_tempMixDevice = new TempMixDevice(parent,false);
    idleSizer->Add(m_idle_tempMixDevice, 0, wxALL | wxEXPAND , 0);
}

void SingleDeviceState::msw_rescale()
{
	int test = 9;
}

void SingleDeviceState::connectEvent()
{
    //device info modify
    //外网数据更新
   MultiComMgr::inst()->Bind(COM_WAN_DEV_INFO_UPDATE_EVENT, &SingleDeviceState::onConnectWanDevInfoUpdate, this);
   //局域网数据更新
   MultiComMgr::inst()->Bind(COM_DEV_DETAIL_UPDATE_EVENT, &SingleDeviceState::onComDevDetailUpdate, this);
   //局域网连接更新
   MultiComMgr::inst()->Bind(COM_CONNECTION_READY_EVENT, &SingleDeviceState::onComConnectReady, this);
   //连接断开
   MultiComMgr::inst()->Bind(COM_CONNECTION_EXIT_EVENT, &SingleDeviceState::onConnectExit, this);
   //get file list info
   MultiComMgr::inst()->Bind(COM_GET_DEV_GCODE_LIST_EVENT, &SingleDeviceState::onFileListUpdate, this);
   //file list file send finished
   MultiComMgr::inst()->Bind(COM_START_JOB_EVENT, &SingleDeviceState::onFileSendFinished, this);
   //lan network download file finished
   MultiComMgr::inst()->Bind(COM_GET_GCODE_THUMB_EVENT, &SingleDeviceState::onLanThumbDownloadFinished, this);

   //local file list
   m_fileListbutton->Bind(wxEVT_LEFT_DOWN, &SingleDeviceState::onFileListClicked, this);

   // time lapse video
   m_timeLapseVideoBtn->Bind(wxEVT_LEFT_DOWN, &SingleDeviceState::onTimeLapseVideoBtnClicked, this);

   //busy button slot
   m_device_info_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e){
       //m_device_info_button->SetIcon("device_idle_file_info");
       m_device_info_button->Refresh();
       if (m_pid == 0x001F && m_busy_G3U_detail) {
           if (m_busy_device_detial) {
               m_busy_device_detial->Hide();
           }
           bool bShow = !m_busy_G3U_detail->IsShown();
           m_busy_G3U_detail->Show(bShow);
           m_busy_G3U_detail->Layout();
           if (bShow) {
               m_device_info_button->SetBackgroundColor(wxColour(217, 234, 255));
           } else {
               m_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
           }
       } else {
           if (m_busy_device_detial) {
               if (m_busy_G3U_detail) {
                   m_busy_G3U_detail->Hide();
               }
               bool bShow = !m_busy_device_detial->IsShown();
               m_busy_device_detial->Show(bShow);
               m_busy_device_detial->Layout();
               if (bShow) {
                   m_device_info_button->SetBackgroundColor(wxColour(217, 234, 255));
               } else {
                   m_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
               }
           }
       }

        if (m_busy_circula_filter) {
            m_busy_circula_filter->Hide();
        }
        if (m_busy_temp_brn) {
            m_busy_temp_brn->Hide();
        }
        if(m_filter_button){
            m_filter_button->SetBackgroundColor(wxColour(255,255,255));
        }
        Layout();
   });

   m_filter_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e){
       wxCommandEvent *event = new wxCommandEvent(EVT_SWITCH_TO_FILETER);
       wxQueueEvent(m_busy_device_detial, event);
       if (m_pid == 0x001F) {
           m_clear_fan_pressed_down = !m_clear_fan_pressed_down;
           if (m_clear_fan_pressed_down) {
               ComClearFanCtrl* clearFan = new ComClearFanCtrl(OPEN);
               Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, clearFan);
               m_filter_button->SetIcon("device_filter");
           } else {
               ComClearFanCtrl* clearFan = new ComClearFanCtrl(CLOSE);
               Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, clearFan);
               m_filter_button->SetIcon("device_filter_offline");
           }
           if (m_busy_circula_filter) {
               m_busy_circula_filter->Hide();
           }
       } else {
           if (m_busy_circula_filter) {
               bool bShow = !m_busy_circula_filter->IsShown();
               m_busy_circula_filter->Show(bShow);
               m_busy_circula_filter->Layout();
               if (bShow) {
                   m_filter_button->SetBackgroundColor(wxColour(217, 234, 255));
               } else {
                   m_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
               }
           }
       }

        if(m_busy_device_detial){
            m_busy_device_detial->Hide();
        }
        if (m_busy_G3U_detail) {
            m_busy_G3U_detail->Hide();
        }
        if (m_busy_temp_brn) {
            m_busy_temp_brn->Hide();
        }
        if(m_device_info_button){
            m_device_info_button->SetBackgroundColor(wxColour(255,255,255));
        }
        Layout();
   });

   m_lamp_control_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
       if (m_lamp_control_button->GetFlashForgeSelected()) {
           // 关灯
           ComLightCtrl *lightctrl = new ComLightCtrl(CLOSE);
           Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, lightctrl);
           m_lamp_control_button->SetIcon("device_lamp_control");
           m_lamp_control_button->Refresh();
           m_lamp_control_button->SetFlashForgeSelected(false);
       } else {
           //开灯
           ComLightCtrl *lightctrl = new ComLightCtrl(OPEN);
           Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, lightctrl);
           m_lamp_control_button->SetIcon("device_lamp_control_press");
           m_lamp_control_button->Refresh();
           m_lamp_control_button->SetFlashForgeSelected(true);
       }
   });
   m_check_printer_status_timer.Bind(wxEVT_TIMER, [this](wxTimerEvent &e) {
       checkPrinterStatus();
   });
   m_check_printer_status_timer.Start(1000);

   m_download_tool.Bind(EVT_FF_DOWNLOAD_FINISHED, &SingleDeviceState::onDownloadImageFinished, this);
}

void SingleDeviceState::on_navigated(wxWebViewEvent &event) 
{
   wxString strInput = event.GetString();
}

void SingleDeviceState::onConnectWanDevInfoUpdate(ComWanDevInfoUpdateEvent &event) 
{
    event.Skip();
    if (m_cur_id == event.id) {
        //当前选中的设备，获取相应数据，更新界面显示
        const com_dev_data_t &data = MultiComMgr::inst()->devData(event.id);
        // 离线判断
        std::string status = data.wanDevInfo.status;
        if (status.compare("offline") == 0) {
            setPageOffline();
        } else {
            fillValue(data,true);
        }
        return;
    }
    
    if (-1 == m_cur_id) {
        const com_dev_data_t &data = MultiComMgr::inst()->devData(event.id);
        if (data.wanDevInfo.serialNumber.compare(m_cur_serial_number) == 0 && data.wanDevInfo.status.compare("offline") != 0) {
            m_cur_id = event.id;
            setCurId(m_cur_id);
        }
        return;
    }
}

void SingleDeviceState::onComDevDetailUpdate(ComDevDetailUpdateEvent &event) 
{
    event.Skip();
    if (m_cur_id == event.id) {
        // 当前选中的设备，获取相应数据，更新界面显示
        bool  valid = false;
        const com_dev_data_t& data  = MultiComMgr::inst()->devData(m_cur_id, &valid);
        fillValue(data);
    }
}

void SingleDeviceState::onComConnectReady(ComConnectionReadyEvent &event) 
{
    event.Skip();
    if (-1 == m_cur_id) {
        const com_dev_data_t &data  = MultiComMgr::inst()->devData(event.id);
        std::string           lan_serial_number = data.lanDevInfo.serialNumber;
        if (!lan_serial_number.empty()  && lan_serial_number.compare(m_cur_serial_number) == 0) {
            m_cur_id = event.id;
            setCurId(m_cur_id);
        }
        return;
    }
}

void SingleDeviceState::onConnectExit(ComConnectionExitEvent &event) 
{ 
    event.Skip(); 
    if (event.id == m_cur_id) {
        setPageOffline();
    }
}

void SingleDeviceState::onTargetTempModify(wxCommandEvent &event) 
{
    event.Skip();
    wxTextCtrl *click_btn = dynamic_cast<wxTextCtrl *>(event.GetEventObject());
    if (event.GetEventType() == wxEVT_TEXT) {
        if (click_btn && m_tempCtrl_top && m_tempCtrl_bottom && m_tempCtrl_mid) {
               m_busy_temp_brn->Show();
               m_busy_temp_brn->Layout();
               if (m_busy_circula_filter) {
               m_busy_circula_filter->Hide();
            }
            if (m_busy_device_detial) {
                m_busy_device_detial->Hide();
            }
            if (m_filter_button) {
                m_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
            }
            if (m_device_info_button) {
                m_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
            }
            Layout();
        }
        double top_temp;
        double bottom_temp;
        m_tempCtrl_top->GetTagTemp().ToDouble(&top_temp);
        m_tempCtrl_bottom->GetTagTemp().ToDouble(&bottom_temp);
        if (top_temp > 280) {
            m_tempCtrl_top->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
            m_tempCtrl_top->SetTagTemp(280, true);
            m_tempCtrl_top->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
        }
        if (bottom_temp > 110) {
            m_tempCtrl_bottom->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
            m_tempCtrl_bottom->SetTagTemp(110, true);
            m_tempCtrl_bottom->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
        }
    }
}

void SingleDeviceState::onModifyTempClicked(wxCommandEvent &event) 
{
    event.Skip();
    m_busy_temp_brn->Hide();
    Layout();
    double top_temp;
    double bottom_temp;
    double mid_temp; 
    bool bTop = m_tempCtrl_top->GetTagTemp().ToDouble(&top_temp);
    if (!bTop) {
       m_tempCtrl_top->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
       m_tempCtrl_top->SetTagTemp(m_right_target_temp, true);
       top_temp = m_right_target_temp;
       m_tempCtrl_top->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this); 
    }
    bool bBottom = m_tempCtrl_bottom->GetTagTemp().ToDouble(&bottom_temp);
    if (!bBottom) {
       m_tempCtrl_bottom->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
       m_tempCtrl_bottom->SetTagTemp(m_plat_target_temp, true);
       bottom_temp = m_plat_target_temp;
       m_tempCtrl_bottom->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
    }
    bool bMid = m_tempCtrl_mid->GetTagTemp().ToDouble(&mid_temp);
    ComTempCtrl* tempCtrl = new ComTempCtrl(bottom_temp, top_temp, 0, mid_temp);
    Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, tempCtrl);
}

void SingleDeviceState::onDevStateChanged(std::string devState, const com_dev_data_t &data)
{
    std::string state = devState; // 状态
    if (data.devDetail->pid == 0x001F) {
        setG3UProductAuthority(*data.devProduct);
    } else {
        setDevProductAuthority(*data.devProduct);
    }
    
    //if (m_cur_dev_state != state) {
        m_cur_dev_state = state;

        double total_weight = data.devDetail->estimatedRightWeight; //材料重量
        char   weight[64];
        ::sprintf(weight, "  %.2f g", total_weight);
        m_material_weight_label->SetLabel(weight);

        if (state == P_READY) {
            m_staticText_device_info->Hide();
            m_clear_button->Hide();
            m_tempCtrl_top->SetTargetTempVis(false);
            m_tempCtrl_bottom->SetTargetTempVis(false);
            m_tempCtrl_mid->SetTargetTempVis(false);
            m_machine_idle_panel->Show();
            m_machine_ctrl_panel->Hide();
            m_busyState_top_gap->Hide();
            m_busyState_bottom_gap->Hide();
            m_panel_idle_text->Show();
            m_panel_separotor8->Show();
            wxString idle_state = _L("idle");
            setTipMessage(idle_state, "#00CD6D", "", false);
            std::string lightStatus = data.devDetail->lightStatus;            
            m_idle_tempMixDevice->setState(1, lightStatus.compare(CLOSE));
            m_cur_print_file_name.clear();
            m_staticText_idle->SetLabel(_L("The current device has \nno printing projects"));
            m_idle_tempMixDevice->setDevProductAuthority(*data.devProduct);
            reInitMaterialPic();
        } else if (state == P_COMPLETED || state == CANCEL) {
            m_staticText_device_info->Hide();
            m_clear_button->Hide();

            m_tempCtrl_top->SetTargetTempVis(true);
            m_tempCtrl_bottom->SetTargetTempVis(true);
            m_tempCtrl_mid->SetTargetTempVis(true);
            m_machine_ctrl_panel->Show();
            m_machine_idle_panel->Hide();
            m_print_button->SetTextColor(wxColor("#999999"));
            m_cancel_button->SetTextColor(wxColor("#999999"));
            m_print_button->Enable(false);
            m_cancel_button->Enable(false);
            m_print_button->SetIcon("device_pause_print_disable");
            m_cancel_button->SetIcon("device_cancel_print_disable");
            wxString compelete_state = _L("completed");
            wxString compelete_info  = _L("Print completed,clean platform!");
            setTipMessage(compelete_state, "#328DFB", compelete_info, true, true);

            m_staticText_time_label->SetLabel(_L("Total Time"));

            double totalTime = data.devDetail->printDuration; // 本次打印耗时
            m_staticText_count_time->SetLabel(convertSecondsToHMS(totalTime));
        } else if (state == P_BUSY) {
            m_staticText_device_info->Hide();
            m_clear_button->Hide();
            m_panel_print_btn->Hide();
            m_scrolledWindow->Hide();
            m_FileList_split_line->Hide();
            m_timeLapseVideoPnl->Hide();
            m_idle_tempMixDevice->Show();
            m_tempCtrl_top->SetTargetTempVis(true);
            m_tempCtrl_bottom->SetTargetTempVis(true);
            m_tempCtrl_mid->SetTargetTempVis(true);
            m_machine_idle_panel->Show();
            m_machine_ctrl_panel->Hide();
            m_panel_idle_text->Hide();
            m_panel_separotor8->Hide();
            m_busyState_top_gap->Show();
            m_busyState_bottom_gap->Show();
            wxString busy_state = _L("busy");
            wxString busy_info = _L("Print cancelled,in cache command");
            setTipMessage(busy_state, "#F9B61C", busy_info, false, false);
            std::string lightStatus = data.devDetail->lightStatus;   
            m_idle_tempMixDevice->setState(1, lightStatus.compare(CLOSE));
            //splitIdleTextLabel();
            m_staticText_idle->SetLabel(_L("The current device has \nno printing projects"));
            m_idle_tempMixDevice->setDevProductAuthority(*data.devProduct);
        } else if (state == P_CALIBRATE) {
            m_staticText_device_info->Hide();
            m_clear_button->Hide();
            m_tempCtrl_top->SetTargetTempVis(true);
            m_tempCtrl_bottom->SetTargetTempVis(true);
            m_tempCtrl_mid->SetTargetTempVis(true);
            m_machine_idle_panel->Show();
            m_machine_ctrl_panel->Hide();
            wxString busy_state = _L("busy");
            wxString busy_info = _L("");
            setTipMessage(busy_state, "#F9B61C", busy_info, false, false);
            std::string lightStatus = data.devDetail->lightStatus;   
            m_idle_tempMixDevice->setState(1, lightStatus.compare(CLOSE));
            //splitIdleTextLabel();
            m_staticText_idle->SetLabel(_L("The current device has \nno printing projects"));
            m_idle_tempMixDevice->setDevProductAuthority(*data.devProduct);
         } else if (state == P_ERROR) {
            m_staticText_device_info->Hide();
            m_clear_button->Hide();
            m_tempCtrl_top->SetTargetTempVis(true);
            m_tempCtrl_bottom->SetTargetTempVis(true);
            m_tempCtrl_mid->SetTargetTempVis(true);
            m_machine_idle_panel->Show();
            m_machine_ctrl_panel->Hide();
            wxString error_state = _L("error");
            std::string error_info  = data.devDetail->errorCode;
            wxString trans_error = FFUtils::converDeviceError(error_info);
            setTipMessage(error_state, "#FB4747", trans_error, !trans_error.empty(), false);
            m_idle_tempMixDevice->setDevProductAuthority(*data.devProduct);
        } else if (state == PAUSE) {
             m_staticText_device_info->Hide();
             m_clear_button->Hide();
            m_tempCtrl_top->SetTargetTempVis(true);
            m_tempCtrl_bottom->SetTargetTempVis(true);
            m_tempCtrl_mid->SetTargetTempVis(true);
            m_machine_ctrl_panel->Show();
            m_machine_idle_panel->Hide();
            wxString print_state = _L("pause");
            setTipMessage(print_state, "#982187");

            m_print_button->Enable(true);
            m_cancel_button->Enable(true);
            m_print_button->SetIcon("device_pause_print");
            m_cancel_button->SetIcon("device_cancel_print");
            m_print_button->SetTextColor(wxColour(51, 51, 51));
            m_cancel_button->SetTextColor(wxColour(51, 51, 51));
            m_print_button->SetLabel(_L("continue print"));
            m_print_button->SetIcon("device_continue_print");
            m_print_button->Refresh();

            m_staticText_time_label->SetLabel(_L("Remaining Time"));
            double estimatedTime = data.devDetail->estimatedTime; // 剩余时间
            m_staticText_count_time->SetLabel(convertSecondsToHMS(estimatedTime));
        } else if (state == P_PAUSING || state == P_HEATING) {
            m_staticText_device_info->Hide();
            m_clear_button->Hide();
            m_tempCtrl_top->SetTargetTempVis(true);
            m_tempCtrl_bottom->SetTargetTempVis(true);
            m_tempCtrl_mid->SetTargetTempVis(true);
            m_machine_ctrl_panel->Show();
            m_machine_idle_panel->Hide();
            wxString print_state = _L("pausing");
            if (state == P_HEATING) {
                print_state = _L("heating");
            }
            setTipMessage(print_state, "#982187");
            m_print_button->SetTextColor(wxColor("#999999"));
            m_cancel_button->SetTextColor(wxColor("#999999"));
            m_print_button->Enable(false);
            m_cancel_button->Enable(false);
            m_print_button->SetIcon("device_pause_print_disable");
            m_cancel_button->SetIcon("device_cancel_print_disable");
            m_print_button->SetLabel(_L("continue print"));
            m_print_button->SetIcon("device_continue_print");
            m_print_button->Refresh();

            m_staticText_time_label->SetLabel(_L("Remaining Time"));
            double estimatedTime = data.devDetail->estimatedTime; // 剩余时间
            m_staticText_count_time->SetLabel(convertSecondsToHMS(estimatedTime));
        }else{
            m_staticText_device_info->Hide();
            m_clear_button->Hide();
            m_tempCtrl_top->SetTargetTempVis(true);
            m_tempCtrl_bottom->SetTargetTempVis(true);
            m_tempCtrl_mid->SetTargetTempVis(true);
            m_machine_ctrl_panel->Show();
            m_machine_idle_panel->Hide();
            wxString print_state = _L("printing");
            setTipMessage(print_state, "#4D54FF");

            m_print_button->Enable(true);
            m_cancel_button->Enable(true);
            m_print_button->SetIcon("device_pause_print");
            m_cancel_button->SetIcon("device_cancel_print");
            m_print_button->SetTextColor(wxColour(51, 51, 51));
            m_cancel_button->SetTextColor(wxColour(51, 51, 51));
            m_print_button->SetLabel(_L("pause print"));
            m_print_button->SetIcon("device_pause_print");
            m_print_button->Refresh();
            m_staticText_time_label->SetLabel(_L("Remaining Time"));
            double estimatedTime = data.devDetail->estimatedTime; // 剩余时间
            m_staticText_count_time->SetLabel(convertSecondsToHMS(estimatedTime));
        }
        Layout();
    //}
}

void SingleDeviceState::onCancelPrint(wxCommandEvent &event)
{
    //event.Skip();
    m_cancel_confirm_page->Close();
    m_cur_printing_ctrl    = 3;
    std::string printState = CANCEL;
    bool valid = false;
    const com_dev_data_t &data = MultiComMgr::inst()->devData(m_cur_id, &valid);
    if (valid) {
         std::string jobId   = Slic3r::GUI::MultiComMgr::inst()->devData(m_cur_id).devDetail->jobId;
         ComJobCtrl *jobCtrl = new ComJobCtrl(jobId, printState);
         Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, jobCtrl);
         // m_cancel_confirm_page->Hide();
    }
}

void SingleDeviceState::onContinuePrint(wxCommandEvent &event)
{
    //event.Skip();
    m_cancel_confirm_page->Close();
    //m_cancel_confirm_page->Hide();
}

void SingleDeviceState::onFileListClicked(wxMouseEvent& event) 
{
    if (m_scrolledWindow && m_scrolledWindow->IsShown()) {
        m_panel_print_btn->Hide();
        m_scrolledWindow->Hide();
        m_FileList_split_line->Hide();

        m_printBtn->Enable(false);
        if (m_curSelectedFileItem) {
            m_curSelectedFileItem->SetPressed(false);
            m_curSelectedFileItem = nullptr;
        }
        m_idle_tempMixDevice->Show();
        m_timeLapseVideoPnl->Hide();
        Layout();
        return;
    }

    if (m_curId_first_Click_fileList) {
         ComGetDevGcodeList* devGcodeList = new ComGetDevGcodeList();
         Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, devGcodeList);
         m_curId_first_Click_fileList = false;
    }

    if (m_scrolledWindow && !m_scrolledWindow->IsShown()) {
         m_idle_tempMixDevice->Hide();
         m_timeLapseVideoPnl->Hide();
         m_scrolledWindow->Scroll(0, 0);
         m_scrolledWindow->Refresh();
         m_scrolledWindow->Show();
         m_panel_print_btn->Show();
         m_FileList_split_line->Show();
    }
    Layout();
}

void SingleDeviceState::onFileListRefreshBtnClicked(wxMouseEvent& event)
{
    m_printBtn->Enable(false);
    m_curSelectedFileItem = nullptr;
    ComGetDevGcodeList* devGcodeList = new ComGetDevGcodeList();
    Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, devGcodeList);
}

void SingleDeviceState::onFileListUpdate(ComGetDevGcodeListEvent& event)
{
    auto setGcodeData = [](FileItem::FileData &fileData, const fnet_gcode_data_t &gcodeData) {
        fileData.wxName = wxString::FromUTF8(gcodeData.fileName);
        fileData.gcodeData.fileName = gcodeData.fileName;
        fileData.gcodeData.thumbUrl = gcodeData.thumbUrl;
        fileData.gcodeData.printingTime = gcodeData.printingTime;
        fileData.gcodeData.totalFilamentWeight = gcodeData.totalFilamentWeight;
        fileData.gcodeData.useMatlStation = gcodeData.useMatlStation;
        fileData.gcodeData.gcodeToolDatas.resize(gcodeData.gcodeToolCnt);
        for (size_t j = 0; j < fileData.gcodeData.gcodeToolDatas.size(); ++j) {
            const fnet_gcode_tool_data_t &gcodeToolData = gcodeData.gcodeToolDatas[j];
            fileData.gcodeData.gcodeToolDatas[j].toolId = gcodeToolData.toolId;
            fileData.gcodeData.gcodeToolDatas[j].slotId = gcodeToolData.slotId;
            fileData.gcodeData.gcodeToolDatas[j].materialName = gcodeToolData.materialName;
            fileData.gcodeData.gcodeToolDatas[j].materialColor = gcodeToolData.materialColor;
            fileData.gcodeData.gcodeToolDatas[j].filemanetWeight = gcodeToolData.filemanetWeight;
        }
    };
    event.Skip();
    if (m_cur_id == event.id) {
        if (event.wanGcodeList.gcodeCnt != 0) {
             const com_gcode_list_t &gcodeList = event.wanGcodeList;
             std::vector<FileItem::FileData> fileDataList(gcodeList.gcodeCnt);
             for (size_t i = 0; i < fileDataList.size(); ++i) {
                 setGcodeData(fileDataList[i], gcodeList.gcodeDatas[i]);
             }
             if (!m_fileItemList.empty()) {
                clearFileList();
                m_scrolledWindow->Scroll(0, 0);
             }
             initFileList(fileDataList);
         }
        if (event.lanGcodeList.gcodeCnt != 0) {
             const com_gcode_list_t &gcodeList = event.lanGcodeList;
             std::vector<FileItem::FileData> fileDataList(gcodeList.gcodeCnt);
             for (int i = 0; i < fileDataList.size(); ++i) {
                 setGcodeData(fileDataList[i], gcodeList.gcodeDatas[i]);
             }
             if (!m_fileItemList.empty()) {
                clearFileList();
                m_scrolledWindow->Scroll(0, 0);
             }
             initFileList(fileDataList);
        }
        if (event.wanGcodeList.gcodeCnt == 0 && event.lanGcodeList.gcodeCnt == 0) {
            if (!m_fileItemList.empty()) {
                clearFileList();
                m_scrolledWindow->Scroll(0, 0);
            }
        }
    }
}

void SingleDeviceState::onFileListPrintBtnClicked(wxMouseEvent& event) 
{
    event.Skip();
    if (m_curSelectedFileItem == nullptr) {
         return;
    }
    PrintDevLocalFileDlg printDevLocalFileDlg(wxGetApp().mainframe);
    wxImage *image;
    wxImage defultImage;
    if (m_curSelectedFileItem->m_data.srcImage.IsOk()) {
        image = &m_curSelectedFileItem->m_data.srcImage;
    } else {
        std::string name = m_curSelectedFileItem->m_data.gcodeData.fileName;
        std::string suffix = name.substr(name.find_last_of(".") + 1);
        ScalableBitmap bmp(&printDevLocalFileDlg, FileItem::getImageNameByType(suffix), FromDIP(117));
        defultImage = bmp.bmp().ConvertToImage();
        image = &defultImage;
    }
    const com_gcode_data_t &gcodeData = m_curSelectedFileItem->m_data.gcodeData;
    com_local_job_data_t jobData;
    jobData.fileName = gcodeData.fileName;
    jobData.printNow = true;
    if (!printDevLocalFileDlg.setupData(m_cur_id, gcodeData, *image)) {
        return;
    }
    if (printDevLocalFileDlg.ShowModal(jobData) != wxID_OK) {
        return;
    }
    ComStartJob* startJob = new ComStartJob(jobData);
    Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, startJob);
    m_printBtn->Enable(false);
    m_refreshBtn->Enable(false);
}

void SingleDeviceState::onFileSendFinished(ComStartJobEvent& event)
{
    event.Skip();
    BOOST_LOG_TRIVIAL(info) << "SingleDeviceState:onFileSendFinished, com_id: " << event.id << ", " << event.ret;
    m_printBtn->Enable(true);
    m_refreshBtn->Enable(true);
}

void SingleDeviceState::onLanThumbDownloadFinished(ComGetGcodeThumbEvent& event) 
{
    event.Skip(); 
    if (event.ret == COM_OK) {
         wxMemoryInputStream stream(event.thumbData.data(), event.thumbData.size());
         wxImage  image(stream, wxBITMAP_TYPE_ANY);
         for (const auto& item : m_fileItemList) {
             if (item->m_data.commandId == event.commandId) {
                item->m_data.srcImage = image;
                item->m_data.scaledImage = image.Rescale(FILELIST_PIC_WIDTH, FILELIST_PIC_HEIGHT);
                break;
             } else {
                continue;
             }
         }
    } else {
         BOOST_LOG_TRIVIAL(info) << "SingleDeviceState:onLanThumbDownloadFinished, com_id: " << event.id << ", " << event.ret;
    }
}

void SingleDeviceState::onTimeLapseVideoBtnClicked(wxMouseEvent& event)
{
    if (m_timeLapseVideoPnl->IsShown()) {
        m_idle_tempMixDevice->Show();
        m_scrolledWindow->Hide();
        m_FileList_split_line->Hide();
        m_panel_print_btn->Hide();
        m_timeLapseVideoPnl->Hide();
    } else {
        m_idle_tempMixDevice->Hide();
        m_scrolledWindow->Hide();
        m_FileList_split_line->Hide();
        m_panel_print_btn->Hide();
        m_timeLapseVideoPnl->updateVideoList();
        m_timeLapseVideoPnl->Show();
    }
    Layout();
}

void SingleDeviceState::onDownloadImageFinished(FFDownloadFinishedEvent& event)
{
    if (!event.succeed) {
        return;
    }
    if (event.taskId == m_download_title_image_task_id) {
        if (m_material_picture != nullptr) {
            wxMemoryInputStream stream(event.data.data(), event.data.size());
            wxImage image(stream, wxBITMAP_TYPE_ANY);
            if (image.IsOk()) {
                m_material_picture->SetImage(image.Rescale(MATERIAL_PIC_WIDTH, MATERIAL_PIC_HEIGHT));
            }
        }
    }
    auto it = m_download_file_list_image_map.find(event.taskId);
    if (it != m_download_file_list_image_map.end()) {
        wxMemoryInputStream stream(event.data.data(), event.data.size());
        wxImage image(stream, wxBITMAP_TYPE_ANY);
        if (image.IsOk()) {
            it->second->m_data.srcImage = image;
            it->second->m_data.scaledImage = image.Rescale(FILELIST_PIC_WIDTH, FILELIST_PIC_HEIGHT);
        }
    }
}

void SingleDeviceState::setTipMessage(const wxString& title, const std::string& titleColor, const wxString& info, bool showInfo, bool showBtn)
{
    m_staticText_device_tip->SetLabel(title); 
    m_staticText_device_tip->SetForegroundColour(wxColour(titleColor));
    m_staticText_device_info->SetLabel(info);
    m_staticText_device_info->SetMinSize(wxSize(FromDIP(394), FromDIP(56)));

    m_staticText_device_info->Show(showInfo);
    m_clear_button->Show(showBtn);
    Layout();
}

void SingleDeviceState::checkPrinterStatus()
{
    if (m_cur_id < 0 || m_block_status_check) {
        return;
    }
    bool valid;
    const fnet_dev_detail_t *devDetail = MultiComMgr::inst()->devData(m_cur_id, &valid).devDetail;
    if (!valid || strcmp(devDetail->status, "error") != 0) {
        return;
    }
    if (strcmp(devDetail->errorCode, "E0088") == 0 || strcmp(devDetail->errorCode, "E0089") == 0) {
        time_t elapsedTime = time(nullptr) - m_status_check_message_show_time;
        if (elapsedTime > 20 || devDetail->errorCode != m_status_check_error_code) {
            m_status_check_error_code = devDetail->errorCode; // 进入事件循环后之前获取的devDetail可能失效
            m_block_status_check = true;
            PrinterErrorMsgDlg(wxGetApp().mainframe, m_cur_id, devDetail->errorCode).ShowModal();
            m_block_status_check = false;
            m_status_check_message_show_time = time(nullptr);
        }
    }
}

wxString SingleDeviceState::convertSecondsToHMS(int totalSeconds)
{
    int hours   = totalSeconds / 3600;
    int remainingSeconds = totalSeconds % 3600;
    int minutes          = remainingSeconds / 60;
    int secs             = remainingSeconds % 60; 

    wxString hoursStr   = wxString::Format("%02d", hours);
    wxString minutesStr = wxString::Format("%02d", minutes);  

    wxString stream = hoursStr.append(_L("h "));
    stream.append(minutesStr);
    stream.append(_L("min "));
    return stream;
    /*    std::ostringstream stream;
        stream << std::setfill('0') << std::setw(1) << hours << _L("h ") << std::setfill('0') << std::setw(2) << minutes << _L("min ");
        return stream.str(); */
}

void SingleDeviceState::fillValue(const com_dev_data_t& data,bool wanDev)
{
    std::string state = data.devDetail->status; // 状态
    if (wanDev) {
        state = data.wanDevInfo.status;
    }
    onDevStateChanged(state, data);
    if (state.compare("printing") == 0) {
        double estimatedTime = data.devDetail->estimatedTime; // 剩余时间
        m_staticText_count_time->SetLabel(convertSecondsToHMS(estimatedTime));
    }

    std::string stram_url = data.devDetail->cameraStreamUrl;
    if (!stram_url.empty() && m_camera_stream_url != data.devDetail->cameraStreamUrl) {
       if (0 == data.connectMode) {
            // 通知设备开流
            ComCameraStreamCtrl *cameraStreamCtrl = new ComCameraStreamCtrl(OPEN);
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, cameraStreamCtrl);
        }

        m_camera_stream_url = data.devDetail->cameraStreamUrl;
        modifyVideoPlayerAddress(m_camera_stream_url);  
    } else if (stram_url.empty()) {
        notifyWebDevOffline();
    }
    std::string device_name = data.devDetail->name;  //设备名
    if (m_cur_dev_name != device_name && !device_name.empty()) {
        m_cur_dev_name  = device_name;
        wxString u8_dev_name = wxString::FromUTF8(device_name);
        wxGCDC dc(this);
        wxString clipName = FFUtils::trimString(dc, u8_dev_name, FromDIP(190));
        m_staticText_device_name->SetLabelText(clipName);
        m_staticText_device_name->SetToolTip(u8_dev_name);
    }

    std::string device_location = data.devDetail->location;//位置
    if (m_cur_dev_location != device_location && !device_location.empty()) {
        m_cur_dev_location = device_location;
        wxString u8_dev_location = wxString::FromUTF8(device_location);
        wxGCDC   dc(this);
        wxString clipName = FFUtils::trimString(dc, u8_dev_location, FromDIP(150));
        m_staticText_device_position->SetLabel(clipName);
        m_staticText_device_position->SetToolTip(u8_dev_location);
    } 

    std::string printFileName = data.devDetail->printFileName; // 文件名
    if (m_cur_print_file_name != printFileName) {
        m_cur_print_file_name       = printFileName;
        //std::string truncatedString = FFUtils::truncateString(printFileName, TEXT_LENGTH);
        wxString wxPrintFileName = wxString::FromUTF8(printFileName);
        wxString truncatedString;
        
        if (wxPrintFileName.Length() > TEXT_LENGTH) {
            truncatedString = wxPrintFileName.SubString(0, TEXT_LENGTH);
            truncatedString.append("...");
        } else {
            truncatedString = wxPrintFileName;
        }    
        m_staticText_file_name->SetLabel(truncatedString);
        m_staticText_file_name->SetToolTip(wxString::FromUTF8(printFileName));
        m_staticText_file_name->Show();
        m_staticText_file_name->Layout();
        Layout();
    }

    setMaterialPic(data);   //图片地址
    if (!wanDev) {
        double printProgress = data.devDetail->printProgress; // 打印进度
        m_progress_bar->SetProgress(printProgress * 100);

        setTempurature(data);

        std::string lightStatus = data.devDetail->lightStatus; // 灯状态
        if (data.devProduct->lightCtrlState == 1 && lightStatus.compare(CLOSE) == 0) {
            m_lamp_control_button->SetIcon("device_lamp_control");
            m_lamp_control_button->Refresh();
            m_lamp_control_button->SetFlashForgeSelected(false);
            m_idle_tempMixDevice->modifyDeviceLampState(false);
        } else if (data.devProduct->lightCtrlState == 1 && lightStatus.compare(OPEN) == 0) {
            m_lamp_control_button->SetIcon("device_lamp_control_press");
            m_lamp_control_button->Refresh();
            m_lamp_control_button->SetFlashForgeSelected(true);
            m_idle_tempMixDevice->modifyDeviceLampState(true);
        }
        std::string internalFanStatus = data.devDetail->internalFanStatus; // 内循环状态
        bool        internal_open     = internalFanStatus.compare(OPEN) ? false : true;
        std::string externalFanStatus = data.devDetail->externalFanStatus; // 外循环状态
        bool        external_open     = externalFanStatus.compare(OPEN) ? false : true;
        m_busy_circula_filter->setAirFilterState(internal_open, external_open);
        m_idle_tempMixDevice->modifyDeviceFilterState(internal_open, external_open);
        if (data.devDetail->pid == 0x001F) {
            std::string clearStatus = data.devDetail->clearFanStatus;
            bool  clear_fan_open = clearStatus.compare(OPEN) ? false : true;
            m_clear_fan_pressed_down   = clear_fan_open;
            if (clear_fan_open) {
                m_filter_button->SetIcon("device_filter");
            } else {
                m_filter_button->SetIcon("device_filter_offline");
            }
            m_idle_tempMixDevice->modifyG3UClearFanState(clear_fan_open);
        }

        std::string rightFilamentType = data.devDetail->rightFilamentType; // 右喷头材料类型
        m_busy_device_detial->setMaterialName(rightFilamentType);
        std::string leftFilamentType = data.devDetail->leftFilamentType; // 左喷头材料类型
        std::string leftFilament     = "L/2: ";
        m_busy_G3U_detail->setMaterialName(leftFilament + leftFilamentType);
        std::string rightFilament = "R/1: ";
        m_busy_G3U_detail->setRightMaterialName(rightFilament + rightFilamentType);
        double currentPrintSpeed = data.devDetail->currentPrintSpeed; // 初始打印速度
        m_busy_device_detial->setInitialSpeed(currentPrintSpeed);
        m_busy_G3U_detail->setInitialSpeed(currentPrintSpeed);
        double printSpeedAdjust = data.devDetail->printSpeedAdjust; // 速度
        if (data.devDetail->printSpeedAdjust != m_last_speed) {
            m_last_speed = data.devDetail->printSpeedAdjust;
            m_busy_device_detial->setSpeed(printSpeedAdjust);
            m_busy_G3U_detail->setSpeed(printSpeedAdjust);
        }
        double zAxisCompensation = data.devDetail->zAxisCompensation; // z轴坐标
        if (data.devDetail->zAxisCompensation != m_last_z_axis_compensation) {
            m_last_z_axis_compensation = data.devDetail->zAxisCompensation;
            m_busy_device_detial->setZAxis(zAxisCompensation);
            m_busy_G3U_detail->setZAxis(zAxisCompensation);
        }
        int printLayer       = data.devDetail->printLayer;       // 当前层
        int targetPrintLayer = data.devDetail->targetPrintLayer; // 目标层数
        m_busy_device_detial->setLayer(printLayer, targetPrintLayer);
        m_busy_G3U_detail->setLayer(printLayer, targetPrintLayer);
        double fillAmount = data.devDetail->fillAmount; // 填充率
        m_busy_device_detial->setFillRate(fillAmount);
        m_busy_G3U_detail->setFillRate(fillAmount);
        double coolingFanSpeed = data.devDetail->coolingFanSpeed; // 喷头风扇
        if (data.devDetail->coolingFanSpeed != m_last_cooling_fan_speed) {
            m_last_cooling_fan_speed = data.devDetail->coolingFanSpeed;
            m_busy_device_detial->setCoolingFanSpeed(coolingFanSpeed);
            m_busy_G3U_detail->setCoolingFanSpeed(coolingFanSpeed);
        }
        double leftCoolingFanSpeed = data.devDetail->coolingFanLeftSpeed; // 左喷头风扇
        if (data.devDetail->coolingFanLeftSpeed != m_last_left_cooling_fan_speed) {
            m_last_left_cooling_fan_speed = data.devDetail->coolingFanLeftSpeed;
            m_busy_G3U_detail->setLeftCoolingFanSpeed(leftCoolingFanSpeed);
        }
        double chamberFanSpeed = data.devDetail->chamberFanSpeed; // 冷却风扇（腔体风扇）
        if (data.devDetail->chamberFanSpeed != m_last_chamber_fan_speed) {
            m_last_chamber_fan_speed = data.devDetail->chamberFanSpeed;
            m_busy_device_detial->setChamberFanSpeed(chamberFanSpeed);
            m_busy_G3U_detail->setChamberFanSpeed(chamberFanSpeed);
        }

        map<int, bool> temp_pid_show_datas;
        temp_pid_show_datas[0x0023] = false;
        temp_pid_show_datas[0x0024] = false;
        temp_pid_show_datas[0x0025] = false;
        temp_pid_show_datas[0x0026] = false;
        temp_pid_show_datas[0x00BB] = false;
        temp_pid_show_datas[0x0027] = true;
        temp_pid_show_datas[0x001F] = true;
        if (m_pid != data.devDetail->pid) {
            for (auto& elem : temp_pid_show_datas) {
                if (data.devDetail->pid == elem.first) {
                    m_tempCtrl_mid->SetReadOnly(!elem.second);
                    m_tempCtrl_mid->Enable(elem.second);
                    m_pid = data.devDetail->pid;
                    auto bitmap_name = FFUtils::getBitmapFileName(m_pid);
                    if (bitmap_name) {
                        m_idle_device_staticbitmap->SetBitmap(create_scaled_bitmap(bitmap_name.ToStdString(), 0, 165));
                    }
                    break;
                }
            }
        }
#if 0
        if (m_pid != data.devDetail->pid) {
            m_pid = data.devDetail->pid;
            m_idle_device_staticbitmap->SetBitmap(create_scaled_bitmap(FFUtils::getBitmapFileName(data.devDetail->pid).ToStdString(), 0, 165));
        }
#endif
        std::string machineType = FFUtils::getPrinterName(data.devDetail->pid);
        std::string nozzleModel = data.devDetail->nozzleModel; // 喷嘴型号
        std::string measure     = data.devDetail->measure;     // 打印尺寸
        measure.append("mm");
        std::string firmwareVersion    = data.devDetail->firmwareVersion; // 固件版本
        std::string serialNubmer       = data.connectMode == 0 ? data.lanDevInfo.serialNumber : data.wanDevInfo.serialNumber; // 序列号
        double      time               = 0;
        if (data.connectMode == 0) {//内网数据以分钟为单位
            time = data.devDetail->cumulativePrintTime / 60;
        } else if (data.connectMode == 1) {//外网小时为单位
            time = data.devDetail->cumulativePrintTime;
        }    
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << time;
        std::string cumulativePrintTime   = oss.str() + " hours";
        double      cumulativeFilament = data.devDetail->cumulativeFilament; // 丝料统计
        wxString    strCumulativeFilament = wxString::Format("%.2f", cumulativeFilament);
        strCumulativeFilament.append("m");
        std::string ipAddr = data.devDetail->ipAddr; // ip地址
        m_idle_tempMixDevice->modifyDeviceInfo(machineType, nozzleModel, measure, firmwareVersion, serialNubmer, cumulativePrintTime,
                                               strCumulativeFilament, ipAddr);
    }
}

void SingleDeviceState::setPageOffline() 
{
   // 离线
    m_cur_id = -1;
    m_material_station->show_material_panel(false);
    m_material_station->setCurId(m_cur_id);
    if (m_panel_idle_text) {
        m_panel_idle_text->Hide();
        m_panel_print_btn->Hide();
        m_scrolledWindow->Hide();
        m_FileList_split_line->Hide();
        m_timeLapseVideoPnl->Hide();
        m_panel_separotor8->Hide();
        m_busyState_top_gap->Show();
        m_busyState_bottom_gap->Show();
    }
    m_idle_tempMixDevice->Show();
    m_machine_idle_panel->Show();
    m_machine_ctrl_panel->Hide();
    notifyWebDevOffline();
    reInit();
}

std::string SingleDeviceState::getCurLanguage() 
{
    AppConfig *app_config = wxGetApp().app_config; 
    return  app_config->get("language");
}

void SingleDeviceState::setMaterialPic(const com_dev_data_t &data)
{
    std::string file_pic_path = data.devDetail->printFileThumbUrl; // 图片地址
    std::string file_pic_name = data.devDetail->printFileName;     // 图片名称

    if (data.connectMode == COM_CONNECT_LAN) {
       if (m_file_pic_name == file_pic_name || file_pic_name.empty() || file_pic_path.empty())
            return;
    } else if (data.connectMode == COM_CONNECT_WAN) {
       if (m_file_pic_url == file_pic_path || file_pic_path.empty())
            return;
    } else {
       return;
    }

    m_file_pic_url  = file_pic_path;
    m_file_pic_name = file_pic_name;
    m_download_title_image_task_id = m_download_tool.downloadMem(m_file_pic_url, 30000, 60000);
}

void SingleDeviceState::setTempurature(const com_dev_data_t& data)
{
    if (m_pid == 0x001F) {
       // 右喷头温度
       double rightTemp = data.devDetail->rightTemp;
       m_tempCtrl_top->SetCurrTemp(rightTemp, true);
       // 右喷头目标温度
       double rightTargetTemp = data.devDetail->rightTargetTemp;
       if (m_right_target_temp != rightTargetTemp) {
            m_right_target_temp = rightTargetTemp;
            m_tempCtrl_top->SetTagTemp(rightTargetTemp, true);
       }

       // 左喷头温度
       double leftTemp = data.devDetail->leftTemp;
       m_tempCtrl_bottom->SetCurrTemp(leftTemp, true);
       // 左喷头目标温度
       double leftTargetTemp = data.devDetail->leftTargetTemp; 
       if (m_plat_target_temp != leftTargetTemp) {
            m_plat_target_temp = leftTargetTemp;
            m_tempCtrl_bottom->SetTagTemp(leftTargetTemp, true);
       }

       // 底板温度
       double platTemp = data.devDetail->platTemp; 

       m_tempCtrl_mid->SetCurrTemp(platTemp, true);
       // 底板目标温度
       double platTempTargetTemp = data.devDetail->platTargetTemp; 
       if (m_chamber_target_temp != platTempTargetTemp) {
            m_chamber_target_temp = platTempTargetTemp;
            m_tempCtrl_mid->SetTagTemp(platTempTargetTemp, true);
       }

       auto     aRightTemp          = static_cast<int>(rightTemp);
       auto     aPlatTemp           = static_cast<int>(leftTemp);
       auto     aChamberTemp        = static_cast<int>(platTemp);
       wxString modify_nozzle_temp  = wxString::Format("%d", aRightTemp);
       wxString modify_plat_temp    = wxString::Format("%d", aPlatTemp);
       wxString modify_chamber_temp = wxString::Format("%d", aChamberTemp);

       m_idle_tempMixDevice->modifyTemp(modify_nozzle_temp, modify_plat_temp, modify_chamber_temp, rightTargetTemp, leftTargetTemp,
                                        platTempTargetTemp);

    } else {
       double rightTemp = data.devDetail->rightTemp; // 右喷头温度
       m_tempCtrl_top->SetCurrTemp(rightTemp, true);
       double rightTargetTemp = data.devDetail->rightTargetTemp; // 右喷头目标温度
       if (m_right_target_temp != rightTargetTemp) {
            m_right_target_temp = rightTargetTemp;
            m_tempCtrl_top->SetTagTemp(rightTargetTemp, true);
       }

       double platTemp = data.devDetail->platTemp; // 平台温度
       m_tempCtrl_bottom->SetCurrTemp(platTemp, true);
       double platTargetTemp = data.devDetail->platTargetTemp; // 平台目标温度
       if (m_plat_target_temp != platTargetTemp) {
            m_plat_target_temp = platTargetTemp;
            m_tempCtrl_bottom->SetTagTemp(platTargetTemp, true);
       }

       double chamberTemp = data.devDetail->chamberTemp; // 腔体温度

       m_tempCtrl_mid->SetCurrTemp(chamberTemp, true);
       double chamberTargetTemp = data.devDetail->chamberTargetTemp; // 腔体目标温度
       if (m_chamber_target_temp != chamberTargetTemp) {
            m_tempCtrl_mid->SetTagTemp(chamberTargetTemp, true);
       }
       auto     aRightTemp          = static_cast<int>(rightTemp);
       auto     aPlatTemp           = static_cast<int>(platTemp);
       auto     aChamberTemp        = static_cast<int>(chamberTemp);
       wxString modify_nozzle_temp  = wxString::Format("%d", aRightTemp);
       wxString modify_plat_temp    = wxString::Format("%d", aPlatTemp);
       wxString modify_chamber_temp = wxString::Format("%d", aChamberTemp);

       m_idle_tempMixDevice->modifyTemp(modify_nozzle_temp, modify_plat_temp, modify_chamber_temp, rightTargetTemp, platTargetTemp,
                                        chamberTargetTemp);
    }
}

void SingleDeviceState::splitIdleTextLabel()
{
    wxGCDC   dc(this);
    wxString multiText;
    Label::split_lines(dc, FromDIP(IDLE_NAME_LENGTH), HAS_NO_PRINTING, multiText);
    m_staticText_idle->SetLabel(multiText);
}

void SingleDeviceState::clearFileList()
{
    for (auto item : m_fileItemList) {
        item->Destroy();
    }
    m_fileItemList.clear();
    m_download_file_list_image_map.clear();
    m_sizer_my_devices->Layout();
}

void SingleDeviceState::initFileList(const std::vector<FileItem::FileData>& fileDataList)
{ 
    for (const auto& fileData : fileDataList) {
       auto mitem = new FileItem(m_scrolledWindow, fileData);
       bool  valid = false;
       const com_dev_data_t& data  = MultiComMgr::inst()->devData(m_cur_id, &valid);
       if (data.connectMode == 0) {
            std::string fileName = mitem->m_data.gcodeData.fileName;
            ComGetGcodeThumb* devGcodeThumb = new ComGetGcodeThumb(fileName);
            mitem->m_data.commandId = devGcodeThumb->commandId();
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, devGcodeThumb);
       } else if (data.connectMode == 1) {
           int taskId = m_download_tool.downloadMem(mitem->m_data.gcodeData.thumbUrl, 30000, 60000);
           m_download_file_list_image_map.emplace(taskId, mitem);
       }
       mitem->Bind(EVT_FILE_ITEM_CLICKED, [mitem, this](wxCommandEvent& event) {
           m_printBtn->Enable(true);
           if (m_curSelectedFileItem == nullptr) {
               m_curSelectedFileItem = mitem;
           } else {
               if (mitem == m_curSelectedFileItem) {
                   return;
               } else {
                   m_curSelectedFileItem->SetPressed(false);
                   m_curSelectedFileItem = mitem;
               }
           }
       });
       m_sizer_my_devices->Add(mitem, 0, wxEXPAND, 0);
       mitem->SetToolTip(fileData.wxName);
       m_fileItemList.emplace_back(mitem);
    }
    int visual_height = fileDataList.size() * FromDIP(45);
    m_scrolledWindow->SetVirtualSize(FromDIP(46), visual_height);
    m_sizer_my_devices->Layout();
}

void SingleDeviceState::onScriptMessage(wxWebViewEvent &evt)
{
    wxString          strInput = evt.GetString();
    std::string       cmd      = evt.GetString().ToUTF8().data();
    std::stringstream ss(cmd), oss;
    pt::ptree         root, response;
    pt::read_json(ss, root);
    if (root.empty())
            return;

    boost::optional<std::string> sequence_id = root.get_optional<std::string>("sequence_id");
    boost::optional<std::string> command     = root.get_optional<std::string>("command");
    if (command.has_value()) {
       std::string command_str = command.value();
       if (command_str.compare("rtsp_player_continue") == 0) {
            // 外网视频继续播放
            ComCameraStreamCtrl *cameraStreamCtrl = new ComCameraStreamCtrl(OPEN);
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, cameraStreamCtrl);
       }
    }
}

}
}
